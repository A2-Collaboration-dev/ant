#include "UnpackerAcqu_FileFormatMk2.h"

#include "UnpackerAcqu.h"
#include "UnpackerAcqu_legacy.h"
#include "UnpackerAcqu_templates.h"

#include "tree/TEvent.h"

#include "RawFileReader.h"

#include "base/Logger.h"

using namespace std;
using namespace ant;
using namespace ant::unpacker;

size_t acqu::FileFormatMk2::SizeOfHeader() const
{
    return sizeof(AcquMk2Info_t);
}

bool acqu::FileFormatMk2::InspectHeader(const vector<uint32_t>& buffer) const
{
    return inspectHeaderMk1Mk2<AcquMk2Info_t>(buffer);
}

void acqu::FileFormatMk2::FillInfo(reader_t &reader, buffer_t &buffer, Info &info) const
{
    const acqu::AcquMk2Info_t* h = reinterpret_cast<const acqu::AcquMk2Info_t*>(buffer.data()+1);

    // fill the Info

    // the C++11 istringstream way is not working on older systems
    // since std::get_time is missing :(
    //istringstream ss_time(h->fTime);
    //ss_time >> get_time(&info.Time,"%a %b %d %T %Y");

    info.Time = std_ext::to_tm(h->fTime, "%a %b %d %T %Y");
    info.Description = std_ext::string_sanitize(h->fDescription);
    info.RunNote = std_ext::string_sanitize(h->fRunNote);
    info.OutFile = std_ext::string_sanitize(h->fOutFile);
    info.RunNumber = static_cast<unsigned>(h->fRun);
    info.RecordLength = static_cast<unsigned>(h->fRecLen);

    // extract the module info
    const unsigned nModules = static_cast<unsigned>(h->fNModule);
    VLOG(9) << "Header says: Have " << nModules << " modules";

    // calculate sizes, ensure that on this platform the extraction works
    static_assert(sizeof(acqu::AcquMk2Info_t) % sizeof(buffer_t::value_type) == 0,
                  "AcquMk2Info_t is not aligned to buffer");
    static_assert(sizeof(acqu::ModuleInfoMk2_t) % sizeof(buffer_t::value_type) == 0,
                  "ModuleInfoMk2_t is not aligned to buffer");
    constexpr size_t infoSize = 1 + // dont't forget the 32bit start marker
                                sizeof(acqu::AcquMk2Info_t)/4; // division by four is ok since static_assert'ed
    constexpr size_t moduleSize = sizeof(acqu::ModuleInfoMk2_t)/4;
    const size_t totalSize =
            infoSize +
            nModules*moduleSize;

    // read enough into buffer
    reader->expand_buffer(buffer, totalSize);

    // add modules to lists
    /// \todo Check for overlapping of raw channels?

    unsigned totalADCs = 0;
    unsigned totalScalers = 0;
    for(unsigned i=0;i<nModules;i++) {
        const auto buffer_ptr = &buffer[infoSize+i*moduleSize];
        const acqu::ModuleInfoMk2_t* m =
                reinterpret_cast<const acqu::ModuleInfoMk2_t*>(buffer_ptr);
        auto it = ModuleIDToString.find(m->fModID);
        if(it == ModuleIDToString.end()) {
            LOG(WARNING) << "Skipping unknown module with ID=0x" << hex << m->fModID << dec;
            continue;
        }

        Info::HardwareModule module;
        module.Identifier = it->second;
        module.Index = m->fModIndex;
        module.Bits = m->fBits;
        module.FirstRawChannel = m->fAmin;

        // ADC and Scaler will be added twice!
        if(m->fModType & acqu::EDAQ_ADC) {
            totalADCs += m->fNChannel;
            module.NRawChannels = m->fNChannel;
            info.ADCModules.emplace_back(module);
        }
        if(m->fModType & acqu::EDAQ_Scaler) {
            totalScalers += m->fNScChannel;
            module.NRawChannels = m->fNScChannel;
            info.ScalerModules.emplace_back(module);
        }
    }
    VLOG(9) << "Header says: Have " << info.ADCModules.size() << " ADC modules with "
            << totalADCs << " channels";
    VLOG(9) << "Header says: Have " << info.ScalerModules.size() << " Scaler modules with "
            << totalScalers << " channels";
}

void acqu::FileFormatMk2::FillFirstDataBuffer(reader_t& reader, buffer_t& buffer)
{
    // finally search for the Mk2Header, this also
    // fills the buffer correctly with the first Mk2DataBuffer (if available)

    // first search at 0x8000 bytes
    if(SearchFirstDataBuffer(reader, buffer, 0x8000))
        return;

    // then search at 10*0x8000 bytes
    if(SearchFirstDataBuffer(reader, buffer, 10*0x8000))
        return;

    // else fail
    throw UnpackerAcqu::Exception("Did not find first data buffer with Mk2 signature");
}


bool acqu::FileFormatMk2::SearchFirstDataBuffer(reader_t& reader, buffer_t& buffer,
                                                size_t offset)
{
    VLOG(9) << "Searching first Mk2 buffer at offset 0x"
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

    // this word should be the Mk2DataBuff...
    if(buffer.back() != acqu::EMk2DataBuff)
        return false;

    // check header info, emit message if there's a problem
    if(info.RecordLength != offset) {
        LogMessage(TUnpackerMessage::Level_t::Warn,
                   std_ext::formatter()
                   << "Record length in header 0x" << hex << info.RecordLength
                   << " does not match true file record length 0x" << offset << dec
                   );
    }

    VLOG(9) << "Found first Mk2 buffer at offset 0x"
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



bool acqu::FileFormatMk2::UnpackDataBuffer(UnpackerAcquFileFormat::queue_t& queue, it_t& it, const it_t& it_endbuffer) noexcept
{
    const size_t buffersize_bytes = 4*distance(it, it_endbuffer);

    // check header word
    if(*it != acqu::EMk2DataBuff) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter() <<
                   "Buffer starts with unexpected header word 0x" << hex << *it << dec
                   );
        return false;
    }
    it++;

    // now loop over buffer contents, aka single events
    unsigned nEventsInBuffer = 0;
    while(it != it_endbuffer && *it != acqu::EBufferEnd) {

        // extract and check serial ID
        const unsigned acquID = *it;
        if(AcquID_last>acquID) {
            VLOG(8) << "Overflow of Acqu EventId detected from "
                    << AcquID_last << " to " << acquID;
        }
        AcquID_last = acquID;
        it++;

        bool good = false;
        UnpackEvent(queue, it, it_endbuffer, good);
        if(!good)
            return false;
        // append the messages to some successfully unpacked event
        AppendMessagesToEvent(queue.back());

        // increment official unique event ID
        ++id;
        nEventsInBuffer++;
    }

    // we reached the end of buffer before finding the acqu::EBufferEnd
    if(it == it_endbuffer) {
        // there's one exception when the sum of the events
        // inside one buffer fit exactly into the buffer,
        // then only the end marker for the event is present
        // but there's no way to detect this properly, since
        // acqu::EEndEvent == acqu::EBufferEnd (grrrrr)
        if(*next(it,-1) == acqu::EEndEvent) {
            LogMessage(TUnpackerMessage::Level_t::Info,
                       std_ext::formatter()
                       << "Buffer was exactly filled with " << nEventsInBuffer
                       << " events, no buffer endmarker present"
                       );
            return true;
        }

        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter()
                   << "Buffer did not have proper end buffer marker:"
                   << "  1. lastword=0x" << hex << setw(8) << setfill('0') << *next(it,-1)
                   << ", 2. lastword=0x" << hex << setw(8) << setfill('0') << *next(it,-2)
                   << ", buffersize_bytes=0x" << buffersize_bytes
                   );
        return false;
    }

    return true;
}

void acqu::FileFormatMk2::UnpackEvent(
        queue_t& queue,
        it_t& it, const it_t& it_endbuffer,
        bool& good
        ) noexcept
{
    // extract and check eventLength
    const unsigned eventLength = *it/sizeof(decltype(*it));

    const auto it_endevent = next(it, eventLength);
    if(it_endevent == it_endbuffer) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter() <<
                   "Event with size 0x" << eventLength
                   << " too big to fit in buffer of remaining size "
                   << distance(it, it_endbuffer)
                   );
        return;
    }
    if(*it_endevent != acqu::EEndEvent) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter() <<
                   "At designated end of event, found unexpected word 0x"
                   << hex << *it_endevent << dec
                   );
        return;
    }
    it++;

    // now work on one event inside buffer

    /// \todo Scan mappings if there's an ADC channel defined which mimicks those blocks

    queue.emplace_back(TEvent::MakeReconstructed(id));
    TEvent::DataPtr& eventdata = queue.back()->Reconstructed;

    hit_storage.clear();
    // there might be more than one scaler block in each event, so
    // so collect them first in this map
    scalers_t scalers;
    while(it != it_endbuffer && *it != acqu::EEndEvent) {
        // note that the Handle* methods move the iterator
        // themselves and set good to true if nothing went wrong

        good = false;

        switch(*it) {
        case acqu::EEPICSBuffer:
            // EPICS buffer
            HandleEPICSBuffer(eventdata->SlowControls, it, it_endevent, good);
            break;
        case acqu::EScalerBuffer:
            // Scaler read in this event
            // there might also be some error buffers in between, args...
            HandleScalerBuffer(
                        scalers,
                        it, it_endevent, good,
                        eventdata->Trigger.DAQErrors);
            break;
        case acqu::EReadError:
            // read error block, some hardware-related information
            HandleDAQError(eventdata->Trigger.DAQErrors, it, it_endevent, good);
            break;
        default:
            // unfortunately, normal hits don't have a marker
            // so we hope for the best at this position in the buffer
            /// \todo Implement better handling of malformed event buffers
            static_assert(sizeof(acqu::AcquBlock_t) <= sizeof(decltype(*it)),
                          "acqu::AcquBlock_t does not fit into word of buffer");
            auto acqu_hit = reinterpret_cast<const acqu::AcquBlock_t*>(addressof(*it));
            // during a buffer, hits can come in any order,
            // and multiple hits with the same ID can happen
            hit_storage.add_item(acqu_hit->id, acqu_hit->adc);
            // decoding hits always works
            good = true;
            it++;
            break;
        }
        // stop immediately in case of problem
        /// \todo Implement more fine-grained unpacking error handling
        if(!good)
            return;
    }

    // check proper EEndEvent
    if(it == it_endbuffer) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter() <<
                   "While unpacking event, found premature end of buffer."
                   );
        return;
    }

    // hit_storage is member variable for better memory allocation performance
    FillDetectorReadHits(eventdata->DetectorReadHits);
    FillSlowControls(scalers, eventdata->SlowControls);

    it++; // go to start word of next event (if any)
}

void acqu::FileFormatMk2::FillDetectorReadHits(vector<TDetectorReadHit>& hits) const noexcept
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
            if(mapping->RawChannels.size() != 1)
                throw UnpackerAcqu::Exception("Not implemented");
            if(mapping->RawChannels[0].Mask != RawChannel_t::NoMask)
                throw UnpackerAcqu::Exception("Not implemented");
            std::vector<std::uint8_t> rawData(sizeof(uint16_t)*values.size());
            std::copy(values.begin(), values.end(),
                      reinterpret_cast<uint16_t*>(std::addressof(rawData[0])));

            hits.emplace_back(mapping->LogicalChannel, move(rawData));
        }
    }

    if(hits.empty()) {
        /// \todo Improve message, maybe add TUnpackerMessage then?
        LOG(WARNING) << "Found event with no hits at all";
    }

}

void acqu::FileFormatMk2::HandleScalerBuffer(
        scalers_t& scalers,
        it_t& it, const it_t& it_end,
        bool& good,
        std::vector<TDAQError>& errors
        ) noexcept
{
    // ignore Scaler buffer marker
    it++;

    if(it==it_end) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "Acqu ScalerBlock only start marker found"
                   );
        return;
    }

    // get the scaler block length in words
    const int scalerLength = *it;
    constexpr int wordsize = sizeof(decltype(*it));
    if(scalerLength % wordsize != 0
       || distance(it,it_end)<scalerLength/wordsize) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "Acqu ScalerBlock length invalid"
                   );
        return;
    }

    const auto it_endscaler = next(it, scalerLength/wordsize);
    if(*it_endscaler != acqu::EScalerBuffer) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   std_ext::formatter()
                   << "Acqu ScalerBlock did not have proper end marker: "
                   << hex << "0x" << *it_endscaler
                   );
        return;
    }
    it++; // skip the length word now

    // fill simple scalers map
    while(it != it_endscaler) {
        // within a scaler block, there might be error blocks
        if(*it == acqu::EReadError) {
            HandleDAQError(errors, it, it_end, good);
            if(!good)
                return;
            good = false;
        }

        // check if enough space left
        if(distance(it, it_endscaler) < 2) {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       "Acqu ScalerBlock contains malformed scaler read"
                       );
            return;
        }

        const uint32_t index = *it++;
        const uint32_t value = *it++;

        scalers[index].push_back(value);
    }

    // this position is only reached when it==it_endscaler
    // skip the scaler buffer end marker
    // already checked above with it_endscaler
    it++;

    good = true;
}

void acqu::FileFormatMk2::FillSlowControls(const scalers_t& scalers,
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

        // fill TSlowControl's payload

        for(const auto& mapping : scaler_mapping.Entries) {
            using RawChannel_t = UnpackerAcquConfig::RawChannel_t<uint32_t>;
            for(const RawChannel_t& rawChannel : mapping.RawChannels) {
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
                    payload.emplace_back(mapping.LogicalChannel.Channel, value);
                }
            }
        }

        VLOG(9) << sc;
    }
}

void acqu::FileFormatMk2::HandleDAQError(std::vector<TDAQError>& errors,
                                         it_t& it, const it_t& it_end,
                                         bool& good) noexcept
{
    // is there enough space in the event at all?
    static_assert(sizeof(acqu::ReadErrorMk2_t) % sizeof(decltype(*it)) == 0,
                  "acqu::ReadErrorMk2_t is not word aligned");
    constexpr int wordsize = sizeof(acqu::ReadErrorMk2_t)/sizeof(decltype(*it));

    if(distance(it, it_end)<wordsize) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "acqu::ReadErrorMk2_t block not completely present in buffer"
                   );
        return;
    }

    // then cast it to data structure
    const acqu::ReadErrorMk2_t* err =
            reinterpret_cast<const acqu::ReadErrorMk2_t*>(addressof(*it));

    // check for trailer word
    if(err->fTrailer != acqu::EReadError) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "Acqu ErrorBlock does not end with expected trailer word"
                   );
        return;
    }

    // lookup the module name
    auto it_modname = acqu::ModuleIDToString.find(err->fModID);
    const string& modname = it_modname == acqu::ModuleIDToString.cend()
                            ? "UNKNOWN" : it_modname->second;

    errors.emplace_back(err->fModID, err->fModIndex, err->fErrCode, modname);

    VLOG_N_TIMES(1000, 2) << errors.back();

    advance(it, wordsize);
    good = true;
}

void acqu::FileFormatMk2::HandleEPICSBuffer(
        std::vector<TSlowControl>& slowcontrols,
        it_t& it, const it_t& it_end,
        bool& good
        ) noexcept
{
    // ignore EPICS buffer marker
    it++;

    // is there enough space in the event at all?
    constexpr size_t wordbytes = sizeof(decltype(*it));
    static_assert(sizeof(acqu::EpicsHeaderInfo_t) % wordbytes == 0,
                  "acqu::EpicsHeaderInfo_t is not word aligned");
    constexpr int headerWordsize = sizeof(acqu::EpicsHeaderInfo_t)/wordbytes;

    if(distance(it, it_end)<headerWordsize) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "acqu::EpicsHeaderInfo_t header not completely present in buffer"
                   );
        return;
    }

    // then cast it to data structure
    const acqu::EpicsHeaderInfo_t* hdr =
            reinterpret_cast<const acqu::EpicsHeaderInfo_t*>(addressof(*it));

    // check the given header info
    // hdr->len aka epicsTotalWords
    // is the maximum size of the EPICS data including the info header
    if(hdr->len % wordbytes != 0) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "EPICS data not word aligned"
                   );
        return;
    }
    const int epicsTotalWords = hdr->len/wordbytes;
    const string epicsModName = hdr->name;
    const size_t nChannels = hdr->nchan;
    const time_t hdr_timestamp = hdr->time;

    if(epicsModName.length()>32) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "acqu::EpicsHeaderInfo_t header has malformed module name"
                   );
        return;
    }
    if(distance(it, it_end)<epicsTotalWords) {
        LogMessage(TUnpackerMessage::Level_t::DataError,
                   "EPICS data not completely present in buffer"
                   );
        return;
    }

    // unfortunately, the EPICS data is no longer word-aligned,
    // so create some additional uint8_t buffer here
    /// \todo Find some way without copying the data?
    /// \todo correct for endian-ness / machine byte ordering?
    const uint8_t* byte_ptr = reinterpret_cast<const uint8_t*>(addressof(*it));
    const vector<uint8_t> bytes(byte_ptr, byte_ptr + epicsTotalWords*wordbytes);

    auto it_byte = bytes.cbegin();

    // then skip the epics header info
    advance(it_byte, headerWordsize*wordbytes);

    // the header told us how many EPICS channels there are,
    // so start decoding them

    for(size_t i=0; i<nChannels; i++) {
        constexpr int chHdrBytes = sizeof(acqu::EpicsChannelInfo_t);
        if(distance(it_byte, bytes.cend()) < chHdrBytes) {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       "EPICS channel header not completely present in buffer"
                       );
            return;
        }

        const acqu::EpicsChannelInfo_t* ch =
                reinterpret_cast<const acqu::EpicsChannelInfo_t*>(addressof(*it_byte));

        if(distance(it_byte, bytes.cend()) < ch->bytes) {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       "EPICS channel payload not completely present in buffer"
                       );
            return;
        }

        auto it_map = map_EpicsTypes.find(ch->type);
        if(it_map == map_EpicsTypes.cend()) {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       "EPICS channel type unknown"
                       );
            return;
        }

        const acqu::EpicsDataTypes_t ch_datatype = it_map->second.first;
        const int16_t ch_typesize = it_map->second.second;
        const int16_t ch_nElements = ch->nelem;
        const string  ch_Name = ch->pvname;

        // another size check for the channel payload
        if(ch->bytes != chHdrBytes + ch_nElements * ch_typesize) {
            LogMessage(TUnpackerMessage::Level_t::DataError,
                       "EPICS channel payload size inconsistent"
                       );
            return;
        }

        // finally we can create the TSlowControl record

        auto record_type = TSlowControl::Type_t::EpicsOneShot;
        auto validity = TSlowControl::Validity_t::Forward;
        stringstream description;
        if(hdr->period<0) {
            record_type = TSlowControl::Type_t::EpicsTimer;
            validity = TSlowControl::Validity_t::Backward;
            description << "Period='" << -hdr->period << " ms'";
        }
        else if(hdr->period>0) {
            record_type = TSlowControl::Type_t::EpicsScaler;
            validity = TSlowControl::Validity_t::Backward;
            description << "Period='" << hdr->period << " scalers'";
        }

        slowcontrols.emplace_back(
                    record_type,
                    validity,
                    hdr_timestamp,
                    ch_Name,
                    description.str()
                    );
        auto& sc = slowcontrols.back();

        // advance to the EPICS channel data (skip channel info header)
        advance(it_byte, chHdrBytes);

        // fill the payload depending on the EPICS data type
        // upcast float to double and short,byte to long

        for(unsigned elem=0;elem<(unsigned)ch_nElements;elem++) {
            switch(ch_datatype) {
            case acqu::EpicsDataTypes_t::BYTE:
                sc.Payload_Int.emplace_back(elem, *it_byte);
                break;
            case acqu::EpicsDataTypes_t::SHORT: {
                const int16_t* value = reinterpret_cast<const int16_t*>(addressof(*it_byte));
                sc.Payload_Int.emplace_back(elem, *value);
                break;
            }
            case acqu::EpicsDataTypes_t::LONG: {
                const int64_t* value = reinterpret_cast<const int64_t*>(addressof(*it_byte));
                sc.Payload_Int.emplace_back(elem, *value);
                break;
            }
            case acqu::EpicsDataTypes_t::FLOAT: {
                static_assert(sizeof(float)==4,"Float should be 4 bytes long");
                const float* value = reinterpret_cast<const float*>(addressof(*it_byte));
                sc.Payload_Float.emplace_back(elem, *value);
                break;
            }
            case acqu::EpicsDataTypes_t::DOUBLE: {
                static_assert(sizeof(double)==8,"Float should be 8 bytes long");
                const double* value = reinterpret_cast<const double*>(addressof(*it_byte));
                sc.Payload_Float.emplace_back(elem, *value);
                break;
            }
            case acqu::EpicsDataTypes_t::STRING: {
                const char* value = reinterpret_cast<const char*>(addressof(*it_byte));
                // interpret as string
                const string value_str(value);
                if((signed)value_str.length()>=ch_typesize) {
                    LogMessage(TUnpackerMessage::Level_t::DataError,
                               "EPICS channel string data too long (no terminating \\0?)"
                               );
                    return;
                }
                sc.Payload_String.emplace_back(elem, value);
                break;
            }
            default:
                throw UnpackerAcqu::Exception("Not implemented");

            } // end switch

            advance(it_byte, ch_typesize);
        }

        VLOG(9) << sc;

    } // end channel loop

    // we successfully parsed the EPICS buffer
    advance(it, epicsTotalWords);

    VLOG(9) << "Successfully parsed EPICS buffer";

    good = true;
}
