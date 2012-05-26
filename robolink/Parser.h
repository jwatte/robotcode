#if !defined(Parser_h)
#define Parser_h

class Parser {
public:
    Parser();
    ~Parser();
    void on_char(char ch);
    virtual int check_buf();

    int bufptr_;
    char buf_[64];
};

#endif  //  Parser_h

