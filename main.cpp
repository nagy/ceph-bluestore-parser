#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <toml++/toml.hpp>

// using namespace std;
using std::string, std::map, std::to_string;

namespace std {
std::string to_string(const toml::table &node) {
  std::ostringstream oss;
  oss << node << std::endl;
  return oss.str();
}
} // namespace std

void dumpArray(std::string_view data, std::ostream &out,
               bool space_and_newline = true) {
  bool collapse = false;
  size_t size = data.size();
  string line;
  for (size_t i = 0; i < size; ++i) {
    const unsigned char ch = data[i];
    {
      std::ostringstream oss;
      oss << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<int>(ch);
      line += oss.str();
    }
    // line += std::printf("%02x", ch);
    if ((i + 1) % 16 == 0 || i == size - 1) {
      if (line == "00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00") {
        if (!collapse) {
          out << "*" << "\n";
        }
        collapse = true;
        line = "";
      } else {
        out << line << (space_and_newline ? "\n" : "");
        collapse = false;
        line = "";
      }
    } else {
      if (space_and_newline) {
        line += " ";
      }
    }
  }
}
string dumpArray(std::string_view data) {
  std::ostringstream ret;
  dumpArray(data, ret, false);
  return ret.str();
}

struct bluefs_super_t {
  unsigned char _unknown[4];
  unsigned char uuid[16];
  unsigned char osd_uuid[16];
  uint64_t version;
  uint32_t block_size;
  // bluefs_fnode_t log_fnode;

  bluefs_super_t() : uuid{0}, osd_uuid{0}, version(0), block_size(0) {};

  toml::table as_toml() const {
    return toml::table{
        {"uuid", dumpArray(std::string_view((char *)uuid, sizeof uuid))},
        {"osd_uuid",
         dumpArray(std::string_view((char *)osd_uuid, sizeof osd_uuid))},
    };
  }
};

struct BlueStoreState {
  string _magic_header, fsid, description;
  map<string, string> _meta; // aka bdev labels
  std::istream &is;
  string _unknown; // storage for unknown bytes
  bluefs_super_t bluefs_super;

  void parse() {
    parse(_magic_header, sizeof "bluestore block device\n" - 1);
    _magic_header.resize(_magic_header.size() - 1); // trim newline at end
    parse(fsid, sizeof "12345678-9012-3456-7890-123456789012\n" - 1);
    fsid.resize(fsid.size() - 1); // trim newline at end

    // skip first few unknown bytes
    parse(_unknown, 38);

    parse(description);
    parse(_meta);

    // skip to BlueFS superblock
    is.seekg(0x1000, is.beg);

    parse(bluefs_super);
    // parse(bluefs_superblock, 0x1000);
  }

  template <typename T> void parse(T &n) {
    this->is.read(reinterpret_cast<char *>(&n), sizeof n);
  }

  template <typename K, typename V> void parse(map<K, V> &map) {
    uint32_t num;
    parse(num);
    for (size_t i = 0; i < num; i++) {
      K key;
      V value;
      parse(key);
      parse(value);
      map.insert({key, value});
    }
  }

  void parse(string &str) {
    uint32_t n;
    parse(n);
    parse(str, n);
  }

  void parse(string &str, uint32_t n) {
    str.resize(n);
    str.assign(n, '\0');
    this->is.read(&str[0], n);
  }

  void parse(bluefs_super_t &super) {
    uint8_t _version;
    parse(_version);
    assert(_version == 2);

    uint8_t _compat_version;
    parse(_compat_version);
    assert(_compat_version == 1);

    parse(super._unknown);
    parse(super.uuid);
    parse(super.osd_uuid);

    parse(super.version);
    assert(super.version == 1);

    parse(super.block_size);
    assert(super.block_size == 4096);
  }

  BlueStoreState(std::istream &stream) : is(stream) {};

  BlueStoreState operator+(const BlueStoreState &other) const {
    BlueStoreState result{this->is};
    result.fsid = other.fsid;
    result.bluefs_super = other.bluefs_super;
    result.description = other.description;
    return result;
  }

  string as_toml() const {
    return std::to_string(toml::table{
        {"fsid", fsid},
        {"bluefs_super", bluefs_super.as_toml()},
        {"description", description},
        {"meta",
         [&]() -> toml::table {
           toml::table ret;
           ret.insert(_meta.begin(), _meta.end());
           return ret;
         }()},
    });
  }

  friend std::ostream &operator<<(std::ostream &os, BlueStoreState const &a) {
    return os << a.as_toml();
  }
};

int main() {
  std::ifstream is("testfile.bin", std::ios::binary);
  BlueStoreState bss(is);
  bss.parse();

  std::cout << (bss) << std::endl;

  // // print unparsed bytes
  // std::cout << "unkown bytes:" << std::endl;
  // dumpArray(bss2._unknown, std::cout);
  // std::cout << "bluefs_superblock:" << std::endl;
  // dumpArray(std::string((char*)&bss2.bluefs_super, sizeof
  // bss2.bluefs_super));
  return 0;
}
