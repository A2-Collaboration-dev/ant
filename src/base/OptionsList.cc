#include "OptionsList.h"
#include "std_ext/string.h"
#include "Logger.h"
#include <cstdlib>

using namespace std;
using namespace ant;

OptionsList::OptionsList(std::shared_ptr<const OptionsList> Parent):
    parent(Parent)
{
}

void OptionsList::SetOption(const string& str, const string delim)
{
    const auto delimiter_pos = str.find(delim);
    if( delimiter_pos != str.npos) {
        const std::string key = str.substr(0, delimiter_pos);
        const std::string val = str.substr(delimiter_pos + delim.length(), str.npos);
        options[key].Value = val;
    } else {
        LOG(WARNING) << "Can't parse option string \"" << str << "\"";
    }
}

void OptionsList::SetOptions(const string& str,const string optdelim, const string valdelim)
{
    for(const auto& option : std_ext::tokenize_string(str, optdelim))
        SetOption(option, valdelim);
}

string OptionsList::GetOption(const string& key) const
{
    auto entry = options.find(key);

    if(entry == options.end()) {
        notfound.insert(key);
        // ask parent
        if(parent) {
            return parent->GetOption(key);
        }
        return "";
    }

    entry->second.Used = true;
    return entry->second.Value;
}

string OptionsList::Flatten() const
{
    stringstream s;
    for(const auto& e : options) {
        s << e.first << "=" << e.second.Value << ":";
    }
    return s.str();
}

std::set<string> OptionsList::GetNotFound() const
{
    return notfound;
}

std::set<string> OptionsList::GetUnused() const
{
    std::set<string> unused;
    for(const auto& e : options)
        if(!e.second.Used)
            unused.insert(e.first);
    return unused;
}

template<>
bool ant::OptionsList::Get<bool>(const string& key, const bool& def_value) const
{
    auto v = GetOption(key);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);

    if(v=="on" || v=="yes" || v=="true" || v=="1" || v=="y") {
        return true;
    }

    if(v=="off" || v=="no" || v=="false" || v=="0" || v=="n") {
        return false;
    }

    return def_value;
}
