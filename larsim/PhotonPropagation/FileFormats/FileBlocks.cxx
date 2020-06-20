/**
 * @file larsim/PhotonPropagation/FileFormats/FileBlocks.cxx
 * @brief  Defines utilities for the definition of binary file formats.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   June 7, 2020
 * @see `larsim/PhotonPropagation/FileFormats/FileBlocks.h`
 */

// library header
#include "larsim/PhotonPropagation/FileFormats/FileBlocks.h"

// framework libraries
#include "cetlib_except/exception.h"

// C/C++ standard library
#include <algorithm> // std::copy_n(), std::copy(), std::fill(), std::min()
#include <ostream>
#include <utility> // std::move()


// -----------------------------------------------------------------------------
// ---  util::fblock::MagicKey
// -----------------------------------------------------------------------------
util::fblock::MagicKey::MagicKey(std::string_view key) {
  std::size_t const nKeyChars = std::min(key.length(), KeySize);
  auto it = std::copy_n(key.cbegin(), nKeyChars, fKey.begin());
  std::fill(it, fKey.end(), '\0');
} // util::fblock::MagicKey::MagicKey(string_view)


// -----------------------------------------------------------------------------
util::fblock::MagicKey::MagicKey(std::string const& key)
  : MagicKey(std::string_view{ key.data(), key.length() })
  {}


// -----------------------------------------------------------------------------
util::fblock::MagicKey::MagicKey(const char* key)
  : MagicKey(std::string_view{ key })
  {}


// -----------------------------------------------------------------------------
std::ostream& util::fblock::operator<<
  (std::ostream& out, util::fblock::MagicKey const& key)
{
  auto const keyChar = key.key();
  std::copy(keyChar.begin(), keyChar.end(), std::ostream_iterator<char>(out));
  return out;
} // util::fblock::operator<< (util::fblock::MagicKey)


// -----------------------------------------------------------------------------
// ---  util::fblock::FileBlock
// -----------------------------------------------------------------------------
util::fblock::FileBlock::FileBlock(util::fblock::BlockInfo const& header)
  : fHeader(header)
  { allocate(); }


// -----------------------------------------------------------------------------
util::fblock::FileBlock::FileBlock(FileBlock&& from)
  : fHeader(std::move(from.fHeader)), fPayload(std::move(from.fPayload))
  { from.fHeader.reset(); }


// -----------------------------------------------------------------------------
util::fblock::FileBlock& util::fblock::FileBlock::operator=
  (FileBlock&& from)
{
  fHeader = std::move(from.fHeader);
  fPayload = std::move(from.fPayload);
  from.fHeader.reset();
  return *this;
} // util::fblock::FileBlock::operator= ()


// -----------------------------------------------------------------------------
void util::fblock::FileBlock::setPayloadSize(std::size_t n) {
  fHeader.fSize = n;
  allocate();
} // util::fblock::FileBlock::setPayloadSize(size_t)


// -----------------------------------------------------------------------------
void util::fblock::FileBlock::allocate() {
  
  fPayload.resize(fHeader.size() / sizeof(BufferStorageUnit_t));
  
} // util::fblock::FileBlock::allocate()


// -----------------------------------------------------------------------------
// ---  util::fblock::String
// -----------------------------------------------------------------------------
util::fblock::String::String(util::fblock::MagicKey const& key)
  : FileBlock{{ key, 0U }}
{
} // util::fblock::String::String(MagicKey)


// -----------------------------------------------------------------------------
util::fblock::String::String(MagicKey const& key, std::string const& s)
  : FileBlock{{ key, static_cast<BlockSize_t>(s.length()) }, s.data()}
{
} // util::fblock::String::String(MagicKey, string)


// -----------------------------------------------------------------------------
void util::fblock::String::set(std::string const& s) {
  setPayloadSize(s.length());
  setPayload(s.data());
} // util::fblock::String::set()


// -----------------------------------------------------------------------------

