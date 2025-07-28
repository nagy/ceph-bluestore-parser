#include <boost/algorithm/hex.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <toml++/toml.hpp>

using std::string, std::map;

void dumpArray(std::string_view data, std::ostream &out,
               bool space_and_newline = true) {
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
    if ((i + 1) % 16 == 0 || i == size - 1) {
      out << line << (space_and_newline ? "\n" : "");
      line = "";
    } else if (space_and_newline) {
      line += " ";
    }
  }
}

map<string, string> convertTomlTableToStringMap(const toml::table &table) {
  map<string, string> result;
  for (auto &&[key, value] : table) {
    const string k = string(key.str());
    const string s = string(*value.value<std::string_view>());
    result.insert({k, s});
  }
  return result;
}

[[nodiscard]] string dumpArray(std::string_view data) {
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

  bluefs_super_t(const toml::table &table) : bluefs_super_t() {
    {
      const string parsed_uuid = *table["uuid"].value<string>();
      const string hash = boost::algorithm::unhex(parsed_uuid);
      assert(hash.size() == 16);
      std::copy(hash.begin(), hash.end(), uuid);
    };

    {
      const string parsed_osd_uuid = *table["osd_uuid"].value<string>();
      const string hash = boost::algorithm::unhex(parsed_osd_uuid);
      assert(hash.size() == 16);
      std::copy(hash.begin(), hash.end(), osd_uuid);
    };
  };

  operator toml::table() const {
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
  string _unknown;           // storage for unknown bytes
  bluefs_super_t bluefs_super;

  void parse(std::istream &is) {
    parse(is, _magic_header, sizeof "bluestore block device\n" - 1);
    _magic_header.resize(_magic_header.size() - 1); // trim newline at end
    parse(is, fsid, sizeof "12345678-9012-3456-7890-123456789012\n" - 1);
    fsid.resize(fsid.size() - 1); // trim newline at end

    // skip first few unknown bytes
    parse(is, _unknown, 38);

    parse(is, description);
    parse(is, _meta);

    // skip to BlueFS superblock
    is.seekg(0x1000, is.beg);

    parse(is, bluefs_super);
    // parse(bluefs_superblock, 0x1000);
  }

  template <typename T> void parse(std::istream &is, T &n) {
    is.read(reinterpret_cast<char *>(&n), sizeof n);
  }

  template <typename K, typename V>
  void parse(std::istream &is, map<K, V> &map) {
    uint32_t num;
    parse(is, num);
    for (size_t i = 0; i < num; i++) {
      K key;
      V value;
      parse(is, key);
      parse(is, value);
      map.insert({key, value});
    }
  }

  void parse(std::istream &is, string &str) {
    uint32_t n;
    parse(is, n);
    parse(is, str, n);
  }

  void parse(std::istream &is, string &str, uint32_t n) {
    str.resize(n);
    str.assign(n, '\0');
    is.read(&str[0], n);
  }

  void parse(std::istream &is, bluefs_super_t &super) {
    parse(is, _version);
    assert(_version == 2);

    parse(is, _compat_version);
    assert(_compat_version == 1);

    parse(is, super._unknown);
    parse(is, super.uuid);
    parse(is, super.osd_uuid);

    parse(is, super.version);
    assert(super.version == 1);

    parse(is, super.block_size);
    assert(super.block_size == 4096);
  }

  BlueStoreState(std::istream &stream) { parse(stream); };
  BlueStoreState(const toml::table &table) {
    description = *table["description"].value<string>();
    fsid = *table["fsid"].value<string>();
    _meta = convertTomlTableToStringMap(*table["meta"].as_table());
    bluefs_super = bluefs_super_t(*table["bluefs_super"].as_table());
  };
  BlueStoreState() {};

  BlueStoreState operator+(const BlueStoreState &other) const {
    BlueStoreState result;
    result.fsid = other.fsid;
    result.bluefs_super = other.bluefs_super;
    result.description = other.description;
    for (const auto &[key, value] : other._meta) {
      result._meta[key] = other._meta.at(key);
    }
    // for (const auto &[key, value] : other._bluefs_files) {}
    return result;
  }

  operator toml::table() const {
    return toml::table{
        {"fsid", fsid},
        {"bluefs_super", (toml::table)bluefs_super},
        {"description", description},
        {"meta",
         [&]() -> toml::table {
           toml::table ret;
           ret.insert(_meta.begin(), _meta.end());
           return ret;
         }()},
    };
  }

  operator std::string() const { return "BlueStoreState[" + description + "]"; }

  friend std::ostream &operator<<(std::ostream &os, BlueStoreState const &a) {
    return os << (toml::table)a;
  }

private:
  uint8_t _version, _compat_version;
};

int main() {
  const BlueStoreState bss(std::cin);
  std::cout << (toml::table)bss << std::endl;

  // To parse from TOML
  // const toml::table tbl = toml::parse(std::cin);
  // const BlueStoreState bss(tbl);
  // std::cout << (toml::table)bss << std::endl;

  // Examples of composition:
  // BlueStoreState s1, s2;
  // const BlueStoreState s3 = s1 + s2;

  // print unparsed bytes
  // std::cout << "unknown bytes:" << std::endl;
  // std::cout << dumpArray(bss2._unknown, std::cout) << std::endl;
  // std::cout << "bluefs_superblock:" << std::endl;
  // std::cout << dumpArray(std::string((char *)&bss2.bluefs_super,
  //                                    sizeof bss2.bluefs_super))
  //           << std::endl;
  return 0;
}
