#pragma once

namespace Jacker {

class Model;
    
void write_jsong(Model &model, const std::string &filepath);
bool read_jsong(Model &model, const std::string &filepath);
    
} // namespace Jacker