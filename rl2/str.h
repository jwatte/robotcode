#if !defined(rl2_str_h)
#define rl2_str_h

template<typename T> void split(std::string const &str, char ch, T &cont) {
    size_t offset = 0;
    while (offset < str.size()) {
        size_t end = str.find(ch, offset);
        if (end == std::string::npos) {
            cont.push_back(str.substr(offset));
            break;
        }
        else {
            cont.push_back(str.substr(offset, end-offset));
            offset = end + 1;
        }
    }
}

#endif  //  rl2_str_h

