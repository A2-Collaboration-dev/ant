
#include "UnpackerAcqu.h" // for UnpackerAcquConfig
#include "UnpackerAcqu_detail.h"
#include "UnpackerAcqu_legacy.h"
#include "UnpackerAcqu_templates.h"
#include "UnpackerAcqu_FileFormatMk1.h"
#include "UnpackerAcqu_FileFormatMk2.h"


#include "tree/TEvent.h"
#include "tree/TEventData.h"

#include "expconfig/ExpConfig.h"
#include "base/Logger.h"
#include "base/std_ext/misc.h"
#include "RawFileReader.h"

#include <algorithm>
#include <exception>
#include <list>
#include <ctime>
#include <iterator> // for std::next
#include <cstdlib>

using namespace std;
using namespace ant;
using namespace ant::unpacker;


unique_ptr<UnpackerAcquFileFormat>
UnpackerAcquFileFormat::Get(const string& filename)
{
    // make a list of all available acqu file format classes
    using format_t = unique_ptr<UnpackerAcquFileFormat>;
    list< format_t > formats;
    formats.push_back(std_ext::make_unique<acqu::FileFormatMk1>());
    formats.push_back(std_ext::make_unique<acqu::FileFormatMk2>());

    // the rather complicated setup of FileFormat implementation
    // is due to the fact that we want to open the file only once,
    // but cannot seek in it (due to compressed files)

    // that means that this factory method needs to know something about

    // how many words shall we read for header inspection?
    const auto it_max =
            max_element(formats.cbegin(), formats.cend(),
                        [] (const format_t& f1, const format_t& f2)
    {
        return f1->SizeOfHeader() < f2->SizeOfHeader();
    });

    /// \todo Check for big-endian/little-endian formatted files?
    // convert bytes to words
    const size_t bufferSize = (*it_max)->SizeOfHeader()/sizeof(uint32_t) + 1;

    // now we try to open the file
    auto reader = std_ext::make_unique<RawFileReader>();
    reader->open(filename);
    vector<uint32_t> buffer(bufferSize);
    reader->read(buffer.data(), buffer.size());

    // then remove all formats which fail
    // to inspect the header in the given buffer
    formats.remove_if([&buffer] (const format_t& f) { return !f->InspectHeader(buffer); });

    if(formats.size() != 1) {
        LOG_IF(formats.size()>1, WARNING) << "More than one matching Acqu format found for file " << filename;
        return nullptr;
    }

    // we found only one candidate
    // give him the reader and the buffer for further processing
    // also fill some header-like events into the queue
    const format_t& format = formats.back();
    format->Setup(move(reader), move(buffer));

    // return the UnpackerAcquFormat instance
    return move(formats.back());
}

UnpackerAcquFileFormat::~UnpackerAcquFileFormat() {}

void acqu::FileFormatBase::Setup(reader_t &&reader_, buffer_t &&buffer_) {
    reader = move(reader_);
    buffer = move(buffer_);

    // let child class fill the info
    FillInfo(reader, buffer, info);

    // create some messages from it
    LogMessage(TUnpackerMessage::Level_t::Info,
               std_ext::formatter()
               << "Acqu Header Info: "
               << "Time='" << std_ext::to_iso8601(std_ext::to_time_t(info.Time)) << "' "
               << "Description='" << info.Description << "' "
               << "RunNote='" << info.RunNote << "' "
               << "OutFile='" << info.OutFile << "' "
               << "RunNumber=" << info.RunNumber << " "
               << "RecordLength=" << info.RecordLength << " "
               << "nModules=" << info.Modules.size() << " "
               );

    // guessing the timestamp from the Acqu header
    // is somewhat more involved...
    const time_t timestamp = GetTimeStamp();

    // construct the unique ID
    id = TID(timestamp, 0u);

    // try to find some config with the id
    auto setup = ExpConfig::Setup::Get(id);
    auto config = dynamic_pointer_cast<UnpackerAcquConfig, ExpConfig::Setup>(setup);
    if(!config) {
        throw ExpConfig::ExceptionNoConfig("Found setup cannot configure this unpacker.");
    }

    // now try to fill the first data buffer
    FillFirstDataBuffer(reader, buffer);
    unpackedBuffers = 0; // not yet unpacked

    // remember the record length size
    trueRecordLength = buffer.size();

    // get the mappings once
    config->BuildMappings(hit_mappings, scaler_mappings);

    // and prepare the member variables for fast unpacking of hits
    for(const UnpackerAcquConfig::hit_mapping_t& hit_mapping : hit_mappings) {
        for(const UnpackerAcquConfig::RawChannel_t<uint16_t>& rawChannel : hit_mapping.RawChannels) {
            const uint16_t ch = rawChannel.RawChannel;
            if(hit_mappings_ptr.size()<=ch)
                hit_mappings_ptr.resize(ch+1);
            hit_mappings_ptr[ch].push_back(addressof(hit_mapping));
        }
    }

}

acqu::FileFormatBase::~FileFormatBase()
{

}

double acqu::FileFormatBase::PercentDone() const
{
    return reader->PercentDone();
}

time_t acqu::FileFormatBase::GetTimeStamp()
{
    // the following calculation assumes
    // Central Europe as timezone
    const char* tz = getenv("TZ");
    setenv("TZ", "Europe/Berlin", 1);
    tzset();

    std_ext::execute_on_destroy restore_TZ([tz] () {
        // restore TZ environment
        if(tz)
            setenv("TZ", tz, 1);
        else
            unsetenv("TZ");
        tzset();
    });


    if(!std_ext::guess_dst(info.Time)) {
        // there's no other way than hardcode the runs
        // recorded when the the MEST->MET transition occurred
        struct manual_dst_t {
            int Year;
            unsigned RunNumber;
            unsigned DST;
            manual_dst_t(unsigned year, unsigned run, unsigned dst) :
                Year(year-1900), RunNumber(run), DST(dst) {}
        };

        const vector<manual_dst_t> manual_dsts = {
            {2014, 6592, 1},
            {2014, 6593, 1},
            {2014, 6594, 0},
            {2014, 6596, 0},
        };

        bool found_item = false;
        for(const manual_dst_t& item : manual_dsts) {
            if(info.Time.tm_year != item.Year)
                continue;
            if(info.RunNumber != item.RunNumber)
                continue;
            found_item = true;
            info.Time.tm_isdst = item.DST;
        }
        if(!found_item)
            throw UnpackerAcqu::Exception(
                    std_ext::formatter() <<
                    "Run " << info.RunNumber << " at " << std_ext::to_iso8601(std_ext::to_time_t(info.Time))
                    << " has unknown DST flag (not found in database)");
    }

    return std_ext::to_time_t(info.Time);
}

void acqu::FileFormatBase::LogMessage(
        TUnpackerMessage::Level_t level,
        const string& msg
        ) const
{

    messages.emplace_back(level, msg);

    unsigned levelnum = 1;
    switch(level) {
    case TUnpackerMessage::Level_t::Info:
        levelnum = 3;
        break;
    case TUnpackerMessage::Level_t::Warn:
        levelnum = 2;
        break;
    default:
        break;
    }

    VLOG(levelnum) << "Buffer n=" << unpackedBuffers
                   << " [TUnpackerMessage] " << messages.back().Message ;

}

void acqu::FileFormatBase::AppendMessagesToEvent(TEvent& event) const
{
    vector<TUnpackerMessage>& u_messages = event.Reconstructed().UnpackerMessages;
    if(u_messages.empty())
       u_messages = move(messages);
    else {
        u_messages.insert(u_messages.end(), messages.begin(), messages.end());
        messages.clear();
    }
}


void acqu::FileFormatBase::FillEvents(queue_t& queue) noexcept
{
    logger::DebugInfo::nUnpackedBuffers = unpackedBuffers;

    // this method never throws exceptions, but just adds TUnpackerMessage to event
    // if something strange while unpacking is encountered

    // we use the buffer as some state-variable
    // if the buffer is already empty now, there is nothing more to read
    if(buffer.empty()) {
        // still issue some TEvent if there are messages left or
        // it's the very first buffer now, then the data consisted of header-only data
        if(!messages.empty() || unpackedBuffers==0) {
            queue.emplace_back(id);
            AppendMessagesToEvent(queue.back());
        }
        return;
    }

    // start parsing the filled buffer
    // however, we fill a temporary queue first
    auto it = buffer.cbegin();
    queue_t queue_buffer;
    if(!UnpackDataBuffer(queue_buffer, it, buffer.cend())) {
        // handle errors on buffer scale
        LOG(WARNING) << "Error while unpacking buffer n=" << unpackedBuffers
                     << ", discarding all unpacked data from buffer.";

        // add an datadiscard message to an empty event
        messages.emplace_back(
                    TUnpackerMessage::Level_t::DataDiscard,
                    "Discarded buffer number {}"
                    );
        messages.back().Payload.push_back(unpackedBuffers);

        queue.emplace_back(id);
        AppendMessagesToEvent(queue.back());
    }
    else {
        // successful, so add all to output
        const int unpackedWords = distance(buffer.cbegin(), it);
        VLOG(7) << "Successfully unpacked " << unpackedWords << " words ("
                << 100.0*unpackedWords/buffer.size() << " %) from buffer ";
        queue.splice(queue.end(), move(queue_buffer));
    }

    unpackedBuffers++;


    // refill the buffer
    try {
        reader->read(buffer.data(), trueRecordLength);
    }
    catch(ant::RawFileReader::Exception e) {
        // clear buffer if there was a problem when reading
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter()
                   << "Error while reading input: " << e.what());
        buffer.clear();
    }

    // check if actually enough bytes were read
    if(reader->gcount() != 4*trueRecordLength) {
        // reached the end of file properly?
        if(reader->gcount() == 0 && reader->eof()) {
            LogMessage(TUnpackerMessage::Level_t::Info,
                       std_ext::formatter()
                       << "Found proper end of file");
        }
        else {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       std_ext::formatter()
                       << "Read only " << reader->gcount()
                       << " bytes, not enough for record length " << 4*trueRecordLength);
        }
        buffer.clear();
    }

    // the above refill might have created messages,
    // and to suppress empty events with messages only,
    // we simply append them to the last event if any present
    if(!queue.empty())
        AppendMessagesToEvent(queue.back());
}

uint32_t acqu::FileFormatBase::GetDataBufferMarker() const
{
    switch(info.Format) {
    case Info::Format_t::Mk1:
        return acqu::EDataBuff;
    case Info::Format_t::Mk2:
        return acqu::EMk2DataBuff;
    }
    return 0;
}

bool acqu::FileFormatBase::SearchFirstDataBuffer(reader_t& reader, buffer_t& buffer,
                                                 size_t offset) const
{
    VLOG(9) << "Searching first data buffer at offset 0x"
            << hex << offset << dec;
    // offset is given in bytes, convert to number of words in uint32_t buffer
    const streamsize nWords = offset/sizeof(uint32_t);
    // read full header record, the file
    // should be at least this long
    reader->expand_buffer(buffer, nWords);
    // if this is a header-only file, the next expand might fail
    try {
        reader->expand_buffer(buffer, nWords+1);
    }
    catch(RawFileReader::Exception e) {
        if(reader->eof()) {
            LOG(WARNING) << "File is exactly " << offset
                         << " bytes long, and contains only header.";
            // indicate eof by empty buffer
            buffer.clear();
            return true;
        }
        // else throw e
        throw e;
    }

    // this word should be the DataBuff marker...
    if(buffer.back() != GetDataBufferMarker())
        return false;

    // check header info, emit message if there's a problem
    if(info.RecordLength != offset) {
        LogMessage(TUnpackerMessage::Level_t::Warn,
                   std_ext::formatter()
                   << "Record length in header 0x" << hex << info.RecordLength
                   << " does not match true file record length 0x" << offset << dec
                   );
    }

    VLOG(9) << "Found first buffer at offset 0x"
            << hex << offset << dec;

    // we finally prepare the first data buffer
    // buffer is at the moment nWords+1 large, and the last word
    // is the first word of the first data buffer
    buffer[0] = buffer.back();

    reader->read(&buffer[1], nWords-1);
    // ensure that nWords-1 are actually read
    streamsize expectedBytes = (nWords-1)*sizeof(uint32_t);
    if(reader->gcount() != expectedBytes) {
        throw UnpackerAcqu::Exception(
                    std_ext::formatter()
                    << "Only " << reader->gcount() << " bytes read from file, but "
                    << expectedBytes << " required for first data buffer"
                    );
    }
    // get rid of duplicate header word at very end
    // now the buffer.size() has exactly the length of one file record
    buffer.resize(nWords);

    return true;
}



void acqu::FileFormatBase::FillDetectorReadHits(const hit_storage_t& hit_storage,
                                                const hit_mappings_ptr_t& hit_mappings_ptr,
                                                vector<TDetectorReadHit>& hits) noexcept
{
    // the order of hits corresponds to the given mappings
    hits.reserve(2*hit_storage.size());

    for(const auto& it_hits : hit_storage) {
        const uint16_t& ch = it_hits.first;
        const std::vector<uint16_t>& values = it_hits.second;

        if(values.empty())
            continue;

        if(ch>=hit_mappings_ptr.size())
            continue;

        for(const UnpackerAcquConfig::hit_mapping_t* mapping : hit_mappings_ptr[ch]) {
            using RawChannel_t = UnpackerAcquConfig::RawChannel_t<uint16_t>;
            if(mapping->RawChannels.size() != 1) {
                LOG(ERROR) << "Not implemented";
                continue;
            }
            if(mapping->RawChannels[0].Mask != RawChannel_t::NoMask()) {
                LOG(ERROR) << "Not implemented";
                continue;
            }
            std::vector<std::uint8_t> rawData(sizeof(uint16_t)*values.size());
            std::copy(values.begin(), values.end(),
                      reinterpret_cast<uint16_t*>(std::addressof(rawData[0])));

            hits.emplace_back(mapping->LogicalChannel, move(rawData));
        }
    }

    if(hits.empty()) {
        /// \todo Improve message, maybe add TUnpackerMessage then?
        LOG(DEBUG) << "Found event with no hits at all";
    }

}

void acqu::FileFormatBase::FillSlowControls(const scalers_t& scalers, const scaler_mappings_t& scaler_mappings,
                                           vector<TSlowControl>& slowcontrols) noexcept
{
    if(scalers.empty())
        return;

    // create slowcontrol items according to mapping

    for(const UnpackerAcquConfig::scaler_mapping_t& scaler_mapping : scaler_mappings) {

        slowcontrols.emplace_back(
                    TSlowControl::Type_t::AcquScaler,
                    TSlowControl::Validity_t::Backward,
                    0, /// \todo estimate some timestamp from ID_lower here?
                    scaler_mapping.SlowControlName,
                    "" // spare the description
                    );
        auto& sc = slowcontrols.back();

        // fill TSlowControl's payload according to entries

        for(const auto& entry : scaler_mapping.Entries) {
            const auto rawChannel = entry.RawChannel;
            const auto it_map = scalers.find(rawChannel.RawChannel);
            if(it_map==scalers.cend())
                continue;
            const std::vector<uint32_t>& values = it_map->second;
            // require strict > to prevent signed/unsigned ambiguity
            using payload_t = decltype(sc.Payload_Int);
            static_assert(sizeof(payload_t::value_type) > sizeof(uint32_t),
                          "Payload_Int not suitable for scaler value");
            // transform the scaler values to KeyValue entries
            // in TSlowControl's Payload_Int
            payload_t& payload = sc.Payload_Int;
            for(const uint32_t& value : values) {
                payload.emplace_back(entry.LogicalChannel, value);
            }
        }

        VLOG(9) << sc;
    }
}
