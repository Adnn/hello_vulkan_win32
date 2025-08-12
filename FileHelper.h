#pragma once


#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>


std::vector<char> readFile(std::filesystem::path aPath)
{
    std::ifstream ifs{aPath, std::ios_base::binary};
    return std::vector<char>{
        std::istreambuf_iterator<char>{ifs},
        std::istreambuf_iterator<char>{std::default_sentinel}
    };
}