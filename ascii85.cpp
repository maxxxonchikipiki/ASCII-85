#include <iostream>
#include <fstream>
#include <cstdint>

static const char HEADER[] = "<~";
static const char TRAILER[] = "~>";

static bool is_whitespace(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void encode(std::ifstream& in, std::ofstream& out) {
    uint8_t buf[4];
    size_t line_len = 2;

    out.write(HEADER, 2);

    for (;;) {
        size_t n = 0;
        for (size_t i = 0; i < 4; ++i) {
            int c = in.get();
            if (c == std::ifstream::traits_type::eof()) {
                break;
            }
            buf[i] = static_cast<uint8_t>(c);
            ++n;
        }
        if (n == 0) {
            break;
        }

        if (n < 4) {
            for (size_t i = n; i < 4; ++i) {
                buf[i] = 0;
            }
        }

        uint32_t word = (static_cast<uint32_t>(buf[0]) << 24)
                      | (static_cast<uint32_t>(buf[1]) << 16)
                      | (static_cast<uint32_t>(buf[2]) << 8)
                      | (static_cast<uint32_t>(buf[3]));

        if (n == 4 && word == 0) {
            if (line_len >= 75) {
                out.put('\n');
                line_len = 0;
            }
            out.put('z');
            ++line_len;
        } else {
            char block[5];
            uint32_t tmp = word;
            for (int i = 4; i >= 0; --i) {
                block[i] = static_cast<char>(tmp % 85 + 33);
                tmp /= 85;
            }

            int output_chars = (n == 4) ? 5 : static_cast<int>(n) + 1;
            for (int i = 0; i < output_chars; ++i) {
                if (line_len >= 75) {
                    out.put('\n');
                    line_len = 0;
                }
                out.put(block[i]);
                ++line_len;
            }
        }
    }

    out.write(TRAILER, 2);
}

static void decode(std::ifstream& in, std::ofstream& out) {
    int c;
    bool in_data = false;
    char buf[5];
    int pos = 0;

    while ((c = in.get()) != std::ifstream::traits_type::eof()) {
        if (!in_data) {
            if (c == '<') {
                int next = in.get();
                if (next == std::ifstream::traits_type::eof()) {
                    break;
                }
                if (next == '~') {
                    in_data = true;
                } else {
                    in.putback(static_cast<char>(next));
                }
            }
            continue;
        }

        if (c == '~') {
            int next = in.get();
            if (next == std::ifstream::traits_type::eof()) {
                break;
            }
            if (next == '>') {
                break;
            }
            in.putback(static_cast<char>(next));
        }

        if (is_whitespace(c)) {
            continue;
        }

        if (c == 'z') {
            if (pos != 0) {
                return;
            }
            uint8_t zero[4] = {0, 0, 0, 0};
            out.write(reinterpret_cast<const char*>(zero), 4);
            continue;
        }

        if (c < 33 || c > 117) {
            return;
        }

        buf[pos] = static_cast<char>(c - 33);
        ++pos;

        if (pos == 5) {
            uint32_t word = 0;
            for (int i = 0; i < 5; ++i) {
                word = word * 85 + static_cast<uint32_t>(static_cast<unsigned char>(buf[i]));
            }

            uint8_t outb[4] = {
                static_cast<uint8_t>(word >> 24),
                static_cast<uint8_t>(word >> 16),
                static_cast<uint8_t>(word >> 8),
                static_cast<uint8_t>(word)
            };
            out.write(reinterpret_cast<const char*>(outb), 4);
            pos = 0;
        }
    }

    if (pos > 0) {
        for (int i = pos; i < 5; ++i) {
            buf[i] = static_cast<char>(84);
        }

        uint32_t word = 0;
        for (int i = 0; i < 5; ++i) {
            word = word * 85 + static_cast<uint32_t>(static_cast<unsigned char>(buf[i]));
        }

        uint8_t outb[4] = {
            static_cast<uint8_t>(word >> 24),
            static_cast<uint8_t>(word >> 16),
            static_cast<uint8_t>(word >> 8),
            static_cast<uint8_t>(word)
        };

        out.write(reinterpret_cast<const char*>(outb), pos - 1);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " -e|-d input output\n";
        return 1;
    }

    const char* mode = argv[1];
    const char* infile = argv[2];
    const char* outfile = argv[3];

    std::ios::openmode in_mode = (mode[1] == 'e') ? std::ios::binary : std::ios::in;
    std::ios::openmode out_mode = (mode[1] == 'e') ? std::ios::out : std::ios::binary;

    std::ifstream in(infile, in_mode);
    if (!in) {
        std::cerr << "Cannot open input file\n";
        return 1;
    }

    std::ofstream out(outfile, out_mode);
    if (!out) {
        std::cerr << "Cannot open output file\n";
        return 1;
    }

    if (mode[1] == 'e') {
        encode(in, out);
    } else if (mode[1] == 'd') {
        decode(in, out);
    } else {
        std::cerr << "Invalid mode. Use -e or -d.\n";
        return 1;
    }

    return 0;
}
