/**
 * @file   larsim/PhotonPropagation/FileFormats/FileBlocks.h
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see    `larsim/PhotonPropagation/FileFormats/FileBlocks.cxx`
 *         `larsim/PhotonPropagation/FileFormats/FileBlocks.tcc`
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_H
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_H


// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/details/initializeConstexprArray.h"

// framework libraries
#include "cetlib_except/exception.h"
#include "gsl/span"

// C/C++ standard library
#include <algorithm> // std::fill()
#include <vector>
#include <array>
#include <fstream>
#include <string_view>
#include <string>
#include <type_traits> // std::remove_cv_t, std::conditional_t<>, ...
#include <cstddef> // std::size_t
#include <cstdint> // std::uint32_t


// -----------------------------------------------------------------------------
/// Definitions for `util::BinaryBlockFile` blocks.
namespace util::fblock {
  
  using BlockWord_t = std::uint32_t; ///< Base type unit in block payload.
  
  ///< Type for unit of information in file block, bytes.
  constexpr std::size_t WordSize = sizeof(BlockWord_t);
  
  /// Type for size of file block in bytes.
  using BlockSize_t = std::uint64_t;
  
  /// Returns whether the specified type has size multiple of a block file word.
  template <typename T>
  constexpr bool alignsWithWord() { return (sizeof(T) % WordSize) == 0; }
  
  // documentation is at definition time
  class MagicKey;
  struct BlockInfo;
  class FileBlock;
  
  constexpr bool operator== (MagicKey const& a, MagicKey const& b);
  
  std::ostream& operator<< (std::ostream& out, MagicKey const& key);
  
  
  // --- BEGIN -- Block file elements ------------------------------------------
  /// @name Block file elements
  /// @{
  
  class Version;
  class Bookmark;
  class String;
  template <typename T> class Number;
  
  /// @}
  // --- END -- Block file elements --------------------------------------------
  
  
  /**
   * @brief Low-level write of data for a block.
   * @tparam BufferType type used in the interpretation of `data` as buffer
   * @tparam T type of data being written (mostly ignored)
   * @tparam Stream type of output stream
   * @param out output stream
   * @param info header of a block the data belongs to (or might); used for size
   * @param data pointer to the beginning of the data to be written
   * @return whether writing was successful
   * 
   * The `data` at the specified memory region is written into the specified
   * output stream. The amount of data being written is determined from the
   * information in the `info` block header.
   * 
   * This function is _not_ guaranteed not to throw exceptions!
   */
  template <typename BufferType = char, typename T, typename Stream>
  bool writeBlockPayload
    (Stream& out, util::fblock::BlockInfo const& info, T const* data);

  /**
   * @brief Low-level write for a file block.
   * @tparam BufferType type used in the interpretation of `data` as buffer
   * @tparam T type of data being written (mostly ignored)
   * @tparam Stream type of output stream
   * @param out output stream
   * @param info header of a block the data belongs to (or might); used for size
   * @param data pointer to the beginning of the data to be written
   * @return whether writing was successful
   * 
   * This function writes a header and then its payload, as specified from its
   * arguments.
   * Use this function only if it is inconvenient to create explicitly a file
   * block object for the data (e.g. because payload is large and should not be
   * duplicated).
   * 
   * This function is _not_ guaranteed not to throw exceptions!
   */
  template <typename BufferType = char, typename T, typename Stream>
  bool writeBlockAndPayload
    (Stream& out, util::fblock::BlockInfo const& info, T const* data);
  
  
  namespace details {
    
    /// Type trait: contains `type` as `T` but with same signedness as `Model`.
    template <typename T, typename Model>
    using make_same_signed_as = std::conditional
      <std::is_signed_v<Model>, std::make_signed_t<T>, std::make_unsigned_t<T>>;
    
    template <typename T, typename Model>
    using make_same_signed_as_t = typename make_same_signed_as<T, Model>::type;
    
    template <std::size_t N>
    constexpr auto NullCharArray()
      { std::array<char, N> a; a.fill('\0'); return a; }
    
  } // namespace details
  
} // namespace util::fblock


// -----------------------------------------------------------------------------
/**
 * @brief Representation of the type of block.
 *
 * The magic key is a sequence of characters (as many as the bytes in the file
 * block word, `WordSize`).
 *
 * It is written in the file as the sequence of key characters, one after the
 * other in the same order.
 */
class util::fblock::MagicKey {
  
    public:
  
  /// Number of bytes in the magic key.
  static constexpr std::size_t KeySize = WordSize;
  
  using Key_t = std::array<char, KeySize>;
  
  /// Constructor: a key full of null characters (yikes!).
  constexpr MagicKey()
    : fKey(util::details::fillConstexprArray<char, KeySize, '\0'>())
    {}
  
  //@{
  /// Constructor: copies the first four characters from `key`.
  MagicKey(std::string_view key);
  
  MagicKey(char const* key);
  
  MagicKey(std::string const& key);
  //@}
  
  
  /// Returns a copy of the key content.
  constexpr Key_t key() const { return fKey; }
  
  
  // --- BEGIN -- Input/output -------------------------------------------------
  /// @name Input/output into streams
  /// @{
  
  /// Reads the value of the key from the specified stream.
  template <typename Stream>
  bool read(Stream& in);
  
  /// Writes the value of the key into the specified stream.
  template <typename Stream>
  bool write(Stream& out) const;
  
  /// @}
  // --- END -- Input/output ---------------------------------------------------
  
  /// Returns a `std::string_view` with the content of the key.
  explicit operator std::string_view() const;
  
  /// Returns a `std::string` with the content of the key.
  explicit operator std::string() const;
  
    private:
  /// The character values of the key.
  std::array<char, KeySize> fKey;
  
}; // util::fblock::MagicKey


namespace util::fblock {
  inline constexpr MagicKey NullKey {}; ///< A null key.
}


// -----------------------------------------------------------------------------
/** 
 * @brief Header of a block in a file.
 * 
 * Contains a magic key (`util::fblock::MagicKey`) and the size of the block
 * payload.
 * It is two words long.
 * 
 * Note that the size may be any positive number, not just a multiple of
 * `WordSize`.
 */
struct util::fblock::BlockInfo {
  
  MagicKey fKey; ///< Block type identification.
  BlockSize_t fSize = 0U; ///< Size of the payload, in bytes.
  
  /// Default constructor: block with a `NullKey` and no payload.
  constexpr BlockInfo() = default;
  
  /// Constructor: block with the specified `key` and `size` (`0` by default).
  constexpr BlockInfo(MagicKey const& key, BlockSize_t size = 0U)
    : fKey(key), fSize(size) {}
  
  
  /// Returns whether this block has the specified key.
  constexpr bool hasKey(MagicKey const& key) const;
  
  /// Returns the key of this block.
  constexpr MagicKey const& key() const { return fKey; }
  
  
  /// Returns the size of the header, in bytes.
  static constexpr BlockSize_t headerSize()
    { return sizeof(util::fblock::BlockInfo); }
  
  /// Returns the size of the payload for this block, in bytes.
  constexpr BlockSize_t size() const { return fSize; }
  
  /// Returns the size of the payload for this block, as number of `T` elements.
  template <typename T>
  constexpr std::size_t sizeAs() const { return size() / sizeof(T); }
  
  /// Returns the size of aligned storage for the payload, in bytes.
  constexpr BlockSize_t alignedSize() const { return alignedSize(size()); }
  
  /// Returns the bytes needed to pad the payload to the aligned storage.
  constexpr BlockSize_t paddingSize() const { return paddingSize(size()); }
  
  
  // --- BEGIN -- Input/output -------------------------------------------------
  /**
   * @name Input/output into streams
   * 
   * I/O of numbers (e.g. size) respects system endian...ness?hood?ity?.
   */
  /// @{
  
  /**
   * @brief Reads the key and then the size of the block from the stream.
   * @tparam Stream type of source stream
   * @param in the stream to read the information from.
   * @return whether reading was successful
   * 
   * On failure, if the header of the block was successfully read the payload
   * is allocated with the correct size but an undefined content.
   * If the header failed reading as well, it is reset to `NullBlockInfo`.
   */
  template <typename Stream>
  bool read(Stream& in);
  
  /// Writes the key and the size of the block into the stream.
  template <typename Stream>
  bool write(Stream& out) const;
  
  /// Skips an amount of bytes in `in` matching the payload of this block.
  template <typename Stream>
  bool skipPayload(Stream& in) const;

  /// @}
  // --- END -- Input/output ---------------------------------------------------
  
  /// Sets this block into to a default-constructed state.
  void reset();
  
  
  /// Returns whether the specified size is aligned with the file block word.
  static constexpr bool isAligned(std::size_t size);
  
  /// Returns the size of the smallest data block at least `size` byes big.
  static constexpr std::size_t alignedSize(std::size_t size);
  
  /// Returns the size of padding to fill the smallest data block containing
  /// at least `size` bytes.
  static constexpr std::size_t paddingSize(std::size_t size)
    { return alignedSize(size) - size; }
  
}; // util::fblock::BlockInfo


namespace util::fblock {
  /// A null block info (null key, no payload).
  inline constexpr BlockInfo NullBlockInfo {};
}


// -----------------------------------------------------------------------------
/**
 * @brief A special block with no payload size and a single word as the version.
 */
class util::fblock::Version {
  
    public:
  
  using Version_t = BlockWord_t; ///< Type representing the version number.
  
  
  // --- BEGIN -- Constructors -------------------------------------------------
  /// Default constructor: `"VERS"` as key, version number `0`.
  Version(): Version({ "VERS" }, Version_t{ 0 }) {}
  
  /// Constructor: sets the key and the version (the latter is `0` if omitted).
  Version(MagicKey const& key, Version_t v = Version_t{ 0 })
    : fKey(key), fVersion(v) {}
  
  // --- END -- Constructors ---------------------------------------------------
  
  /// Returns the key of this version block.
  constexpr MagicKey const& key() const { return fKey; }
  
  //@{
  /// Access to the version.
  Version_t& version() { return fVersion; }
  Version_t version() const { return fVersion; }
  Version_t operator() () const { return version(); }
  //@}
  
  /// Sets the version.
  void set(Version_t v) { fVersion = v; }
  
  /// Returns whether this version has the specified key.
  bool hasKey(MagicKey const& key) const { return fKey == key; }
  
  
  // --- BEGIN -- Input/output -------------------------------------------------
  /**
   * @name Input/output into streams
   * 
   * I/O of numbers (e.g. size) respects system endian...ness?hood?ity?.
   */
  /// @{
  
  /**
   * @brief Reads the key and then the version from the stream.
   * @tparam Stream type of source stream
   * @param in the stream to read the information from.
   * @return whether reading was successful
   */
  template <typename Stream>
  bool read(Stream& in);
  
  /// Writes the key and the version into the stream.
  template <typename Stream>
  bool write(Stream& out) const;
  
  /// @}
  // --- END -- Input/output ---------------------------------------------------
  
    private:
  
  MagicKey fKey; ///< Block type identification.
  BlockWord_t fVersion = 0U; ///< Version number.
  
}; // util::fblock::Version


// -----------------------------------------------------------------------------
/// Block in a file.
class util::fblock::FileBlock {
  
  /// Element of the storage buffer.
  using BufferStorageUnit_t = char;
  
  /// Size of a single element of the buffer.
  static constexpr std::size_t BufferCharSize = sizeof(BufferStorageUnit_t);
  
  /// Storage type for the buffer.
  using BufferStorage_t = std::vector<BufferStorageUnit_t>;
  
  BlockInfo fHeader;
  BufferStorage_t fPayload; ///< The data of the block.
  
    public:
  
  /// Type for the generic data buffer of the block.
  using Buffer_t = char*;
  
  /// Type for the generic data buffer of the block (constant).
  using ConstBuffer_t = char const*;
  
  
  /// Default constructor: null block with empty payload.
  FileBlock() = default;
  
  /// Constructor: prepares the block but does not initialize the payload.
  FileBlock(BlockInfo const& header);
  
  /// Constructor: copies the data from `payload`.
  template <typename T>
  FileBlock(BlockInfo const& header, T const* payload);
  
  
  FileBlock(FileBlock const&) = default;
  FileBlock(FileBlock&& from);
  
  FileBlock& operator= (FileBlock const&) = default;
  FileBlock& operator= (FileBlock&& from);
  
  
  // --- BEGIN -- Access to key ------------------------------------------------
  /// @name Access to key
  /// @{
  
  /// Returns the key of this block.
  MagicKey const& key() const { return fHeader.key(); }
  
  /// Returns the key of this block matches `key`.
  bool hasKey(MagicKey const& key) const { return fHeader.hasKey(key); }
  
  /// @}
  // --- END -- Access to key --------------------------------------------------
  
  
  // --- BEGIN -- Access to size -----------------------------------------------
  /// @name Access to payload size
  /// @{
  
  /// Returns the size of the payload for this block, in bytes.
  constexpr BlockSize_t size() const { return fHeader.size(); }
  
  /// Returns the size of the payload for this block, as number of `T` elements.
  template <typename T>
  constexpr std::size_t sizeAs() const { return fHeader.sizeAs<T>(); }
  
  /// Returns the size of aligned storage for the payload, in bytes.
  constexpr BlockSize_t alignedSize() const { return fHeader.alignedSize(); }
  
  /// Returns the bytes needed to pad the payload to the aligned storage.
  constexpr BlockSize_t paddingSize() const { return fHeader.paddingSize(); }
  
  /// @}
  // --- END -- Access to size -------------------------------------------------
  
  
  // --- BEGIN -- Access to payload --------------------------------------------
  /// @name Access to payload
  /// @{
  
  //@{
  /// Returns a pointer to the payload data.
  ConstBuffer_t payloadBuffer() const
    { return static_cast<ConstBuffer_t>(fPayload.data()); }
  Buffer_t payloadBuffer()
    { return static_cast<Buffer_t>(fPayload.data()); }
  //@}
  
  
  //@{
  /// Returns a pointer to the payload, recast as pointer to type `T`.
  template <typename T>
  T const* payload() const
    { return reinterpret_cast<T const*>(payloadBuffer()); }
  template <typename T>
  T* payload() { return reinterpret_cast<T*>(payloadBuffer()); }
  //@}
  
  //@{
  /// Returns the payload buffer reinterpreted as a sequence of `T`.
  template <typename T>
  gsl::span<T const> payloadSequence() const
    { return { payload<T>(), static_cast<gsl::index>(sizeAs<T>()) }; }
  template <typename T>
  gsl::span<T> payloadSequence()
    { return { payload<T>(), static_cast<gsl::index>(sizeAs<T>()) }; }
  //@}
  
  //@{
  /// Returns the payload, recast as type `T`.
  template <typename T>
  T const& payloadAs() const { return *payload<T>(); }
  template <typename T>
  T& payloadAs() { return *payload<T>(); }
  //@}
  
  //@{
  /// Copies data from `buffer` into the payload, to fill it.
  template <typename T>
  void setPayload(T const* buffer);
  //@}
  
  //@{
  /// Makes a payload with `n` bytes of data from `buffer`.
  template <typename T>
  void setPayload(std::size_t n, T const* buffer);
  //@}
  
  /// @}
  // --- END -- Access to payload ----------------------------------------------
  
  
  // --- BEGIN -- Input/output -------------------------------------------------
  /**
   * @name Input/output into streams
   * 
   * I/O of numbers (e.g. size) respects system endian...ness?hood?ity?.
   */
  /// @{
  
  /**
   * @brief Reads the block (header and payload).
   * @tparam Stream type of source stream
   * @param in the stream to read the data from.
   * @return whether reading was successful
  /// @see `readPayload()`
   * 
   * On failure, the value of the object is restored to default-constructed.
   */
  template <typename Stream>
  bool read(Stream& in);
  
  /// Writes the block (header and payload).
  /// @see `writePayload()`
  template <typename Stream>
  bool write(Stream& out) const;
  
  /**
   * @brief Reads the block payload.
   * @tparam Stream type of source stream
   * @param in the stream to read the data from.
   * @return whether reading was successful
   * 
   * The amount of data extracted from the input stream `in` is at least as
   * large as the payload itself, but it can be a bit larger to align to
   * the size of the word.
   * 
   * On failure, the payload content is undefined (the size is still correct).
   */
  template <typename Stream>
  bool readPayload(Stream& in);
  
  /// Skips the block payload in the `in` stream.
  template <typename Stream>
  bool skipPayload(Stream& in) const;
  
  /// Writes the block payload into the `out` stream.
  template <typename Stream>
  bool writePayload(Stream& out) const;
  
  /// @}
  // --- END -- Input/output ---------------------------------------------------
  
  
    protected:
  
  /// Allocates memory enough to store a payload of size `n`. Not initialized.
  void setPayloadSize(std::size_t n);
  
  /// Allocates memory enough to store the payload. Not initialized.
  void allocate();
  
    private:
  static_assert
    (BufferCharSize == 1U, "Code is not supporting for non-1 buffers");

}; // util::fblock::FileBlock


// -----------------------------------------------------------------------------
/**
 * @brief Block with no payload (just a key).
 * 
 * This object may be used to set marks inside the file.
 */
class util::fblock::Bookmark: public FileBlock {
  
    public:
  
  /// Default constructor: block with a `NullKey`.
  Bookmark() = default;
  
  /// Constructor: bookmark with the specified `key`.
  Bookmark(MagicKey const& key): FileBlock(BlockInfo{key}) {}
  
}; // util::fblock::Bookmark



// -----------------------------------------------------------------------------
/**
 * @brief A file block containing a string of characters.
 * 
 * The allocated size of the string is always a multiple of `BlockWord_t`.
 */
class util::fblock::String: public FileBlock {
  
    public:
  
  /// Default constructor: null key, empty string.
  String() = default;
  
  /// Initializes the block data with the specified magic and an empty string.
  String(MagicKey const& key);
  
  /// Initializes the block data with the specified magic and string.
  String(MagicKey const& key, std::string const& s);
  
  /// Sets the content of the block to a copy of the specified string.
  void set(std::string const& s);
  
  // @{
  /// Returns the content of the string as a C++ string view.
  std::string_view string_view() const { return makeView(begin(), end()); }
  operator std::string_view() const { return string_view(); }
  std::string_view operator() () const { return string_view(); }
  // @}
  
  //@{
  /// Returns the content of the string as a C++ string.
  std::string str() const { return { begin(), end() }; }
  explicit operator std::string() const { return str(); }
  //@}
  
  
  // --- BEGIN -- Iterators ----------------------------------------------------
  /// @name Iterators
  /// @{
  
  // @{
  /// Returns a pointer to the first character of the string in the payload.
  char const* begin() const;
  char const* cbegin() const { return begin();}
  // @}
  
  // @{
  /// Returns a pointer after the last character of the string in the payload.
  char const* end() const;
  char const* cend() const { return end();}
  // @}
  
  /// @}
  // --- END -- Iterators ------------------------------------------------------
  
  
    private:
  
  /// Returns a view of data from `b` to the last non-null character before `e`.
  template <typename Iter>
  static std::string_view makeView(Iter b, Iter e);
  
}; // util::fblock::String


// -----------------------------------------------------------------------------
/**
 * @brief A file block containing a number.
 * @tparam T type of number
 * 
 * Only types whose size is smaller than or multiple of `WordSize`
 * are accepted.
 */
template <typename T>
class util::fblock::Number: public FileBlock {
  
    public:
  using Value_t = std::remove_cv_t<T>; // Type of contained value.
  
    private:
  /// Size of the contained value.
  static constexpr std::size_t ValueSize = sizeof(Value_t);
  
  /// Type  for storage of `Value_t` in the file.
  using StoredType_t = std::conditional_t<
    (sizeof(Value_t) < WordSize) && std::is_integral_v<Value_t>,
    details::make_same_signed_as_t<BlockWord_t, Value_t>,
    Value_t
    >;
  
    public:
  
  /// Import constructors.
  using util::fblock::FileBlock::FileBlock;
  
  
  /// Default constructor: null key, default-constructed value.
  Number(): Number(NullKey) {}
  
  /// Initializes the block data with the specified magic and string.
  Number(MagicKey const& key, Value_t v = Value_t{});
  
  
  //@{
  /// Access to the stored value.
  std::enable_if_t<std::is_same_v<Value_t, StoredType_t>, Value_t&> value();
  Value_t value() const;
  Value_t operator() () const { return value(); }
  //@}
  
  /// Set the payload to the specified value `v`.
  void set(Value_t v);
  
    private:
  
  // remove direct access to the payload; only `set()` will perform that.
  template <typename U>
  void setPayload(U const* buffer) { FileBlock::setPayload(buffer); }
  
  template <typename U>
  void setPayload(std::size_t n, U const* buffer) = delete;
  
  void setPayloadSize(std::size_t) = delete;
  
  
  static_assert(
    (ValueSize < WordSize) || util::fblock::alignsWithWord<Value_t>(),
    "The type of Number<> must have size multiple of WordSize"
    );
  static_assert(
    std::is_integral_v<Value_t> || (ValueSize >= WordSize),
    "Number<> supports non-integer types only if with size multiple of WordSize"
    );
  
}; // util::fblock::Number<>



// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------

static_assert(util::fblock::alignsWithWord<util::fblock::BlockSize_t>(),
  "Block file size type must be a multiple of word size.");

#include "larsim/PhotonPropagation/FileFormats/FileBlocks.tcc"


// -----------------------------------------------------------------------------
// ---  inline implementation
// -----------------------------------------------------------------------------
// ---  util::fblock::MagicKey
// -----------------------------------------------------------------------------
inline util::fblock::MagicKey::operator std::string_view() const {
  auto it = fKey.rbegin();
  auto const kend = fKey.rend();
  std::size_t n = 4U;
  while (it != kend) {
    if (*it++ != '\0') break;
    --n;
  }
  return { fKey.begin(), n };
} // util::fblock::MagicKey::operator std::string_view()


// -----------------------------------------------------------------------------
inline util::fblock::MagicKey::operator std::string() const
  { auto const sv = std::string_view(*this); return { sv.begin(), sv.end() }; }


// -----------------------------------------------------------------------------
inline constexpr bool util::fblock::operator==
  (MagicKey const& a, MagicKey const& b)
{
  auto const& akey = a.key();
  auto const& bkey = b.key();
  auto ia = akey.cbegin(), ib = bkey.cbegin();
  auto const aend = akey.cend();
  while (ia != aend) {
    if (*ia++ != *ib++) return false;
  }
  return true;
} // util::fblock::operator== (MagicKey)


// -----------------------------------------------------------------------------
// ---  util::fblock::BlockInfo
// -----------------------------------------------------------------------------
inline constexpr bool util::fblock::BlockInfo::hasKey(MagicKey const& key) const
  { return fKey == key; }


// -----------------------------------------------------------------------------
inline constexpr bool util::fblock::BlockInfo::isAligned(std::size_t size) {
  return (size % WordSize) == 0;
} // util::fblock::BlockInfo::isAligned(size)


// -----------------------------------------------------------------------------
inline constexpr std::size_t util::fblock::BlockInfo::alignedSize
  (std::size_t size)
{
  
  std::size_t const excess = size % WordSize;
  return (excess == 0)? size: (size - excess + WordSize);
  
} // util::fblock::BlockInfo::alignedSize(size)


// -----------------------------------------------------------------------------
inline void util::fblock::BlockInfo::reset() { *this = NullBlockInfo; }


// -----------------------------------------------------------------------------
// ---  util::fblock::String
// -----------------------------------------------------------------------------
inline char const* util::fblock::String::begin() const
  { return payload<char>(); }


// -----------------------------------------------------------------------------
inline char const* util::fblock::String::end() const
  { return begin() + sizeAs<char>(); }


// -----------------------------------------------------------------------------


#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_H
