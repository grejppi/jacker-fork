#pragma once

#include "model.hpp"
#include "json/json.h"

namespace Jacker {

//=============================================================================

class JSongWriter {
public:
    typedef std::map<Pattern *, int> Pattern2IdMap;
    Pattern2IdMap pattern2id;

    void collect(Json::Value &root, PatternEvent &event);
    void collect(Json::Value &root, Pattern &pattern);
    void collect(Json::Value &root, Song::Event &event);
    void collect(Json::Value &root, Song &song);
    void collect(Json::Value &root, Loop &loop);
    void collect(Json::Value &root, Track &track);
    void collect(Json::Value &root, Model &model);

    void write(Json::Value &root, const std::string &filepath);    
};

//=============================================================================

class JSongReader {
public:
    // so we can resolve by index
    std::vector<Pattern *> patterns;

    bool extract(const Json::Value &value, std::string &target);
    bool extract(const Json::Value &value, int &target);
    bool extract(const Json::Value &value, bool &target);

    void build(const Json::Value &root, Pattern::Event &event);    
    void build(const Json::Value &root, Pattern &pattern);    
    bool build(const Json::Value &root, Song::Event &event);    
    void build(const Json::Value &root, Song &song);
    void build(const Json::Value &root, Loop &loop);    
    void build(const Json::Value &root, Track &track);
    void build(const Json::Value &root, Model &model);

    bool read(Json::Value &root, const std::string &filepath);
};

//=============================================================================

void write_jsong(Model &model, const std::string &filepath);
bool read_jsong(Model &model, const std::string &filepath);
    
//=============================================================================

} // namespace Jacker