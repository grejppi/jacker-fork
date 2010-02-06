
#include "model.hpp"

#include "json/json.h"
#include <iostream>
#include <fstream>
#include <cassert>

namespace Jacker {

class JSongReader {
public:
    // so we can resolve by index
    std::vector<Pattern *> patterns;

    bool extract(const Json::Value &value, std::string &target) {
        if (!value.isString())
            return false;
        target = value.asString();
        return true;
    }

    bool extract(const Json::Value &value, int &target) {
        if (!value.isInt())
            return false;
        target = value.asInt();
        return true;
    }
    
    void build(const Json::Value &root, Pattern::Event &event) {
        extract(root["frame"], event.frame);
        extract(root["channel"], event.channel);
        extract(root["param"], event.param);
        extract(root["value"], event.value);        
    }
    
    void build(const Json::Value &root, Pattern &pattern) {
        extract(root["name"], pattern.name);
        int length = 64;
        if (extract(root["length"], length))
            pattern.set_length(length);
        int channel_count = 1;
        if (extract(root["channel_count"], channel_count))
            pattern.set_channel_count(channel_count);
        
        const Json::Value events = root["events"];
        for (size_t i = 0; i < events.size(); ++i) {
            Pattern::Event event;
            build(events[i], event);
            pattern.add_event(event);
        }
        
        patterns.push_back(&pattern);
    }
    
    bool build(const Json::Value &root, Track::Event &event) {
        extract(root["frame"], event.frame);
        int pattern_index = -1;
        if (!extract(root["pattern"], pattern_index))
            return false;
        if ((pattern_index < 0)||(pattern_index >= patterns.size()))
            return false;
        event.pattern = patterns[pattern_index];
        return true;
    }
    
    void build(const Json::Value &root, Track &track) {
        extract(root["name"], track.name);
        extract(root["order"], track.order);
        
        const Json::Value events = root["events"];
        for (size_t i = 0; i < events.size(); ++i) {
            Track::Event event;
            if (build(events[i], event))
                track.add_event(event);
        }
    }

    void build(const Json::Value &root, Model &model) {
        model.reset();
        extract(root["end_cue"], model.end_cue);
        extract(root["frames_per_beat"], model.frames_per_beat);
        extract(root["beats_per_bar"], model.beats_per_bar);
        extract(root["beats_per_minute"], model.beats_per_minute);
        
        const Json::Value patterns = root["patterns"];
        for (size_t i = 0; i < patterns.size(); ++i) {
            Pattern &pattern = model.new_pattern();
            build(patterns[i], pattern);
        }
        
        const Json::Value tracks = root["tracks"];
        for (size_t i = 0; i < tracks.size(); ++i) {
            Track &track = model.new_track();
            build(tracks[i], track);
        }
    }

    bool read(Json::Value &root, const std::string &filepath) {
        Json::Reader reader;
        std::ifstream inp(filepath.c_str());
        bool result = reader.parse(inp, root);
        if (!result) {
            std::cout << "Error parsing JSong: " << reader.getFormatedErrorMessages();
        }
        inp.close();
        if (root["format"] != "jacker-song")
            return false;
        return result;
    }
};
    
class JSongWriter {
public:
    typedef std::map<Pattern *, int> Pattern2IdMap;
    Pattern2IdMap pattern2id;

    void collect(Json::Value &root, PatternEvent &event) {
        root["frame"] = event.frame;
        root["channel"] = event.channel;
        root["param"] = event.param;
        root["value"] = event.value;
    }

    void collect(Json::Value &root, Pattern &pattern) {
        root["name"] = pattern.name;
        root["length"] = pattern.get_length();
        root["channel_count"] = pattern.get_channel_count();
        
        Json::Value events;
        
        for (Pattern::iterator iter = pattern.begin(); 
             iter != pattern.end(); ++iter) {
            Json::Value event;
            collect(event, iter->second);
            if (!event.empty())
                events.append(event);
        }
        
        if (!events.empty()) {
            root["events"] = events;
        }
    }
    
    void collect(Json::Value &root, TrackEvent &event) {
        Pattern2IdMap::iterator iter = pattern2id.find(event.pattern);
        if (iter == pattern2id.end())
            return;
        root["frame"] = event.frame;
        root["pattern"] = iter->second;
    }
    
    void collect(Json::Value &root, Track &track) {
        root["name"] = track.name;
        root["order"] = track.order;
        
        Json::Value events;
        
        for (Track::iterator iter = track.begin();
            iter != track.end(); ++iter) {
            Json::Value event;
            collect(event, iter->second);
            if (!event.empty())
                events.append(event);
        }
        
        if (!events.empty()) {
            root["events"] = events;
        }
    }

    void collect(Json::Value &root, Model &model) {
        root["format"] = "jacker-song";
        root["end_cue"] = model.end_cue;
        root["frames_per_beat"] = model.frames_per_beat;
        root["beats_per_bar"] = model.beats_per_bar;
        root["beats_per_minute"] = model.beats_per_minute;
        
        Json::Value patterns;
        
        int index = 0;
        for (PatternList::iterator iter = model.patterns.begin(); 
             iter != model.patterns.end(); ++iter) {
            Json::Value pattern;
            collect(pattern, *(*iter));
            if (!pattern.empty()) {
                patterns.append(pattern);
                pattern2id.insert(Pattern2IdMap::value_type(*iter,index));
                index++;
            }
        }
        
        if (!patterns.empty()) {
            root["patterns"] = patterns;
        }
        
        Json::Value tracks;
        for (TrackArray::iterator iter = model.tracks.begin();
            iter != model.tracks.end(); ++iter) {
            Json::Value track;
            collect(track, *(*iter));
            if (!track.empty()) {
                tracks.append(track);
            }
        }
        
        if (!tracks.empty()) {
            root["tracks"] = tracks;
        }
    }

    void write(Json::Value &root, const std::string &filepath) {
        assert(!root.empty());            
        std::ofstream out(filepath.c_str());
        assert(out);
        out << root;
        out.close();
    }
    
};

bool read_jsong(Model &model, const std::string &filepath) {
    JSongReader reader;
    Json::Value root;
    if (!reader.read(root, filepath))
        return false;
    reader.build(root,model);
    return true;
}

void write_jsong(Model &model, const std::string &filepath) {
    JSongWriter writer;
    Json::Value root;
    writer.collect(root,model);
    writer.write(root,filepath);
}
    
} // namespace Jacker