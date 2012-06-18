#if !defined(args_h)
#define args_h

template<typename T>
void trim(T &s)
{
    size_t front = 0;
    size_t back = s.size();
    while (front < back && isspace(s[front]))
    {
        ++front;
    }
    while (back > front && isspace(s[back-1]))
    {
        --back;
    }
    s = s.substr(front, back-front);
}


template<typename SetOption>
bool read_config_file(std::string const &path, SetOption const &so)
{
    std::ifstream ifile(path.c_str(), std::ifstream::in | std::ifstream::binary);
    if (!ifile)
    {
        std::cerr << path << ": file not readable" << std::endl;
        return false;
    }
    char buf[1024];
    int lineno = 0;
    bool allOk = true;
    while (!ifile.eof())
    {
        buf[0] = 0;
        ifile.getline(buf, 1024);
        ++lineno;
        size_t off = 0;
        while (isspace(buf[off]))
        {
            ++off;
        }
        if (buf[0] && buf[0] != '#')
        {
            std::string s(&buf[off]);
            size_t off = s.find('=');
            if (off == std::string::npos)
            {
                std::cerr << path << ":" << lineno << ": missing value for: " << s << std::endl;
                return false;
            }
            std::string v(s.substr(off+1));
            s = s.substr(0, off);
            trim(s);
            trim(v);
            if (!so(s, v))
            {
                allOk = false;
            }
        }
    }
    return allOk;
}

template<typename SetOption>
bool parse_args(int &argc, char const ** &argv, SetOption const &so)
{
    bool allOk = true;
    bool readFile = false;
    while (argc > 0)
    {
        if ((*argv)[0] == '-')
        {
            std::string s;
            if ((*argv)[1] == '-')
            {
                s = (*argv) + 2;
            }
            else
            {
                s = (*argv) + 1;
            }
            std::string v;
            if (s.find('=') == std::string::npos)
            {
                if (argc <= 1)
                {
                    std::cerr << "missing value for: " << s << std::endl;
                    return false;
                }
                v = *argv;
                --argc;
                ++argv;
            }
            else
            {
                v = s.substr(s.find('=') + 1);
                s = s.substr(0, s.find('='));
            }
            if (!so(s, v))
            {
                allOk = false;
            }
        }
        else
        {
            readFile = true;
            if (!read_config_file(*argv, so))
            {
                allOk = false;
            }
        }
        --argc;
        ++argv;
    }
    if (!readFile)
    {
        std::string s(getenv("HOME"));
        s += "/.config/robolink.cfg";
        readFile = true;
        read_config_file(s.c_str(), so);
    }
    return allOk;
}

#endif

