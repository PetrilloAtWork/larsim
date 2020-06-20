/**
 * @file   larsim/PhotonPropagation/FileFormats/FileBlocks.tcc
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see    `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h`
 * 
 * This is a template implementation file.
 */

#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_TCC
#define LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_TCC


#ifndef LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_H
# error "This header must not be included directly; include `larsim/PhotonPropagation/FileFormats/FileBlocks.h` instead"
#endif // !LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_H


// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/details/initializeConstexprArray.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard library
#include <algorithm> // std::copy_n()
#include <iterator> // std::distance(), std::next()
#include <utility> // std::move()
#include <cassert>


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
namespace util::fblock::details {
  
  // ---------------------------------------------------------------------------
  /// Skips `n` _bytes_ from the stream `in`.
  template <typename Stream>
  bool wasteData(Stream& in, std::size_t n) {
    return bool(in.seekg(n, std::ios_base::cur));
  } // wasteData()
  
  
  // ---------------------------------------------------------------------------
  /// Reads `n` _bytes_ from the stream `in` into `buffer`.
  template <typename Stream, typename T>
  bool readData(Stream& in, T* buffer, std::size_t n) {
    // return value converted into boolean;
    // for istream::read(), the return value is the stream itself (`in`),
    // and its conversion to boolean (`!fail()`) is `true` if there was no error
    return
      bool(in.read(reinterpret_cast<typename Stream::char_type*>(buffer), n));
  } // readData()
  
  
  // ---------------------------------------------------------------------------
  /// Reads `n` values of type `T` from the stream `in` into `buffer`.
  template <typename Stream, typename T>
  bool readObj(Stream& in, T* buffer, std::size_t n)
    { return readData(in, buffer, sizeof(T) * n); }
  
  
  // ---------------------------------------------------------------------------
  /// Reads the `value` from the stream `in`.
  template <typename Stream, typename T>
  bool readObj(Stream& in, T& value) { return readObj(in, &value, 1U); }
  
  
  // ---------------------------------------------------------------------------
  /// Writes `n` _bytes_ from `buffer` into the stream `out`.
  template <typename Stream, typename T>
  bool writeData(Stream& out, T const* buffer, std::size_t n) {
    // return value converted into boolean;
    // for ostream::write(), the return value is the stream itself (`out`),
    // and its conversion to boolean (`!fail()`) is `true` if there was no error
    return bool(
      out.write(reinterpret_cast<typename Stream::char_type const*>(buffer), n)
      );
  } // writeData()
  
  
  // ---------------------------------------------------------------------------
  /// Writes `n` values of type `T` from `buffer` into the stream `out`.
  template <typename Stream, typename T>
  bool writeObj(Stream& out, T const* buffer, std::size_t n)
    { return writeData(out, buffer, sizeof(T) * n); }
  
  
  // ---------------------------------------------------------------------------
  /// Writes the `value` into the stream `out`.
  template <typename Stream, typename T>
  bool writeObj(Stream& out, T const& value)
    { return writeObj(out, &value, 1U); }
  
  
  // ---------------------------------------------------------------------------
  template <typename T, std::size_t N>
  constexpr std::array<T, N> makeNullBuffer()
    { return util::details::fillConstexprArray<T, N, T{}>(); }
  
  
  // ---------------------------------------------------------------------------
  
} // namespace util::fblock::details


// -----------------------------------------------------------------------------
template <typename BufferType /* = char */, typename T, typename Stream>
bool util::fblock::writeBlockPayload
  (Stream& out, util::fblock::BlockInfo const& info, T const* data)
{
  
  // this buffer has enough null characters to fill a whole block word:
  constexpr auto NullBuffer = details::makeNullBuffer<BufferType, WordSize>();
  
  return details::writeData(out, data, info.size())
    && details::writeData(out, NullBuffer.data(), info.paddingSize());
  
} // util::fblock::writeBlockPayload()


// -----------------------------------------------------------------------------
template <typename BufferType /* = char */, typename T, typename Stream>
bool util::fblock::writeBlockAndPayload
  (Stream& out, util::fblock::BlockInfo const& info, T const* data)
{
  
  return info.write(out) && writeBlockPayload(out, info, data);
  
} // util::fblock::writeBlockAndPayload()


// -----------------------------------------------------------------------------
// --- util::fblock::MagicKey
// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::MagicKey::read(Stream& in) {
  
  // return value converted into boolean;
  // for istream::read(), the return value is the stream itself (`in`),
  // and its conversion to boolean (`!fail()`) is `true` if there was no error.
  return details::readObj(in, fKey.data(), KeySize);
  
} // util::fblock::MagicKey::read()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::MagicKey::write(Stream& out) const {
  
  return details::writeObj(out, fKey.data(), KeySize);
  
} // util::fblock::MagicKey::write()


// -----------------------------------------------------------------------------
// ---  util::fblock::Version
// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::Version::read(Stream& in) {
  
  if (fKey.read(in) && details::readObj(in, fVersion)) return true;
  
  // restore to default-constructed if any failure happened
  *this = {};
  return false;
  
} // util::fblock::Version::read()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::Version::write(Stream& out) const {
  
  return fKey.write(out) && details::writeObj(out, fVersion);
  
} // util::fblock::Version::write()


// -----------------------------------------------------------------------------
// ---  util::fblock::BlockInfo
// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::BlockInfo::read(Stream& in) {
  
  if (fKey.read(in) && details::readObj(in, fSize)) return true;
  
  reset();
  return false;
  
} // util::fblock::BlockInfo::read()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::BlockInfo::write(Stream& out) const {
  
  return fKey.write(out) && details::writeObj(out, fSize);
  
} // util::fblock::BlockInfo::write()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::BlockInfo::skipPayload(Stream& in) const
  { return details::wasteData(in, alignedSize()); }


// -----------------------------------------------------------------------------
// ---  util::fblock::FileBlock
// -----------------------------------------------------------------------------
template <typename T>
util::fblock::FileBlock::FileBlock
  (util::fblock::BlockInfo const& header, T const* payload)
  : FileBlock(header)
{
  setPayload(payload);
} // util::fblock::FileBlock::FileBlock(BlockInfo, T const*)


// -----------------------------------------------------------------------------
template <typename T>
void util::fblock::FileBlock::setPayload(T const* buffer) {
  std::copy_n(
    reinterpret_cast<BufferStorageUnit_t const*>(buffer),
    fPayload.size() / BufferCharSize, fPayload.begin()
    );
} // util::fblock::FileBlock::setPayload()


// -----------------------------------------------------------------------------
template <typename T>
void util::fblock::FileBlock::setPayload(std::size_t n, T const* buffer) {
  setPayloadSize(n);
  setPayload(buffer);
} // util::fblock::FileBlock::setPayload(size_t)


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::FileBlock::read(Stream& in) {
  
  if (fHeader.read(in)) return readPayload(in);
  fPayload.clear();
  return false;
  
} // util::fblock::FileBlock::read()


// -----------------------------------------------------------------------------
/// Writes the block (header and payload).
template <typename Stream>
bool util::fblock::FileBlock::write(Stream& out) const {
  
  assert(fPayload.size() * sizeof(BufferStorageUnit_t) == fHeader.size());
  return
    writeBlockAndPayload<BufferStorageUnit_t>(out, fHeader, fPayload.data());
  
} // util::fblock::FileBlock::write()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::FileBlock::readPayload(Stream& in) {
  
  allocate();
  return
    details::readData(in, fPayload.data(), fPayload.size() * BufferCharSize)
    && details::wasteData(in, fHeader.paddingSize());
  
} // util::fblock::FileBlock::readPayload()


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::FileBlock::skipPayload(Stream& in) const
  { return fHeader.skipPayload(in); }


// -----------------------------------------------------------------------------
template <typename Stream>
bool util::fblock::FileBlock::writePayload(Stream& out) const {
  
  assert(fPayload.size() * sizeof(BufferStorageUnit_t) == fHeader.size());
  return writeBlockPayload<BufferStorageUnit_t>(out, fHeader, fPayload.data());
  
} // util::fblock::FileBlock::writePayload()


// -----------------------------------------------------------------------------
// --- util::fblock::String
// -----------------------------------------------------------------------------
template <typename Iter>
std::string_view util::fblock::String::makeView(Iter b, Iter e) {
  
  while (e != b) {
    auto p = std::prev(e);
    if (*p != '\0') break;
    e = p;
  } // while
  
//   return { b, e }; // C++20
  return { &*b, static_cast<std::size_t>(std::distance(b, e)) };
  
} // util::fblock::String::makeView()


// -----------------------------------------------------------------------------
// --- util::fblock::Number<>
// -----------------------------------------------------------------------------
template <typename T>
util::fblock::Number<T>::Number(MagicKey const& key, Value_t value)
  : FileBlock{{ key, ValueSize }}
  { set(value); }


// -----------------------------------------------------------------------------
template <typename T>
auto util::fblock::Number<T>::value() 
  -> std::enable_if_t<std::is_same_v<Value_t, StoredType_t>, Value_t&>
  { return payloadAs<Value_t>(); }


// -----------------------------------------------------------------------------
template <typename T>
auto util::fblock::Number<T>::value() const -> Value_t
  { return static_cast<Value_t>(payloadAs<StoredType_t>()); }


// -----------------------------------------------------------------------------
template <typename T>
void util::fblock::Number<T>::set(Value_t v) {
  auto const storedValue = static_cast<StoredType_t>(v); // convert
  FileBlock::setPayload(&storedValue);
} // util::fblock::Number<>::set()


// -----------------------------------------------------------------------------

#endif // LARSIM_PHOTONPROPAGATION_FILEFORMATS_FILEBLOCKS_TCC
