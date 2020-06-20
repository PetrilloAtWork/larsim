/**
 * @file    BinaryBlockFile_test.cc
 * @brief   Unit test for `util::BinaryBlockFile`.
 * @author  Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date    June 19, 2020
 * @see     `larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h`
 */

// Boost libraries
#define BOOST_TEST_MODULE ( BinaryBlockFile_test )
#include <cetlib/quiet_unit_test.hpp> // BOOST_AUTO_TEST_CASE()
#include <boost/test/test_tools.hpp> // BOOST_CHECK()

// LArSoft libraries
#include "larsim/PhotonPropagation/FileFormats/BinaryBlockFile.h"

// framework libraries
#include "gsl/span"

// C/C++ standard libraries
#include <numeric> // std::iota()
#include <fstream>
#include <string>
#include <tuple>


using namespace std::string_literals;

//------------------------------------------------------------------------------
std::string const TestFilePath = "test.dat";
enum class tf: std::size_t {
    Version
  , String1
  , Num1
  , Num2
  , Mark1
  , Data
  , String2
  , EndMark
}; // tf
std::array const TestKeys {
  "TEST"s, // Version
  "STR1"s, // String1
  "NUM1"s, // Num1
  "NUM2"s, // Num2
  "MRK1"s, // Mark1
  "DATA"s, // Data
  "STR2"s, // String2
  "DONE"s  // EndMark
};
std::tuple const TestData {
  2U,               // [Version] "TEST"
  "String data 1"s, // [String1] "STR1"
  1U,               // [Num1]    "NUM1"
  -2LL,             // [Num2]    "NUM2"
  nullptr,          // [Mark1]   "MRK1"
  6U,               // [Data]    "DATA"
  "String data 2"s, // [String2] "STR2"
  nullptr           // [EndMark] "DONE"
};

template <tf N>
constexpr decltype(auto) getKey() { return std::get<std::size_t(N)>(TestKeys); }
template <tf N>
constexpr decltype(auto) getData() { return std::get<std::size_t(N)>(TestData); }


template <tf DataIndex>
std::vector<double> makeTestData() {
  std::vector<double> data(getData<DataIndex>());
  std::iota(data.begin(), data.end(), 1.0);
  return data;
} // makeTestData()


//------------------------------------------------------------------------------
void WriterTest() {
  
  using namespace util::fblock;
  
  std::vector<double> const data = makeTestData<tf::Data>();
  
  util::BinaryBlockFile destFile
    (TestFilePath, std::ios::out | std::ios::trunc);
  
  destFile.writeBlock<Version>(getKey<tf::Version>(), getData<tf::Version>());

  destFile.writeBlock<String>(getKey<tf::String1>(), getData<tf::String1>());
  
  destFile.writeBlock<Number<unsigned int>>
    (getKey<tf::Num1>(), getData<tf::Num1>());
  
  destFile.writeBlock<Number<long long int>>
    (getKey<tf::Num2>(), getData<tf::Num2>());
  
  destFile.writeBlock<Bookmark>(getKey<tf::Mark1>());
  
  destFile.writeBlockAndPayload
    ({ getKey<tf::Data>(), getData<tf::Data>() * sizeof(double) }, data.data());
  
  destFile.writeBlock<String>(getKey<tf::String2>(), getData<tf::String2>());
  
  destFile.writeBlock<Bookmark>(getKey<tf::EndMark>());
  
} // WriterTest()


//------------------------------------------------------------------------------
void TestFileTest() {
  //
  // minimal test to check that the content of the file has been written
  // correctly; of course it also tests a lot of reading...
  //
  using namespace util::fblock;
  
  std::vector<double> const expectedData = makeTestData<tf::Data>();
  
  util::BinaryBlockFile srcFile({ TestFilePath, std::ios::in });
  
  auto const& version = srcFile.readVersion(getKey<tf::Version>());
  static_assert(std::is_same_v<std::decay_t<decltype(version)>, Version>);
  BOOST_CHECK_EQUAL(version.version(), getData<tf::Version>());
  
  auto const& string1 = srcFile.readBlock<String>(getKey<tf::String1>());
  static_assert(std::is_same_v<std::decay_t<decltype(string1)>, String>);
  BOOST_CHECK_EQUAL(std::string(string1), getData<tf::String1>());
  
  auto const& number1
    = srcFile.readBlock<Number<unsigned int>>(getKey<tf::Num1>());
  static_assert
    (std::is_same_v<std::decay_t<decltype(number1)>, Number<unsigned int>>);
  BOOST_CHECK_EQUAL(number1.value(), getData<tf::Num1>());
  
  auto const& number2
    = srcFile.readBlock<Number<long long int>>(getKey<tf::Num2>());
  static_assert
    (std::is_same_v<std::decay_t<decltype(number2)>, Number<long long int>>);
  BOOST_CHECK_EQUAL(number2.value(), getData<tf::Num2>());
  
  srcFile.skipBlock(getKey<tf::Mark1>(), "bookmark 1"_bd);
  
  auto const& data = srcFile.readBlock(getKey<tf::Data>(), "data"_bd);
  static_assert(std::is_same_v<std::decay_t<decltype(data)>, FileBlock>);
  BOOST_CHECK_EQUAL(data.size(), getData<tf::Data>() * sizeof(double));
  auto const& dataColl = data.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataColl.begin(), dataColl.end(),
    expectedData.begin(), expectedData.end()
    );
  
  auto const& string2 = srcFile.readBlock<String>(getKey<tf::String2>());
  static_assert(std::is_same_v<std::decay_t<decltype(string2)>, String>);
  BOOST_CHECK_EQUAL(std::string(string2), getData<tf::String2>());
  
  srcFile.skipBlock(getKey<tf::EndMark>(), "end"_bd);
  
  BOOST_CHECK_THROW(srcFile.skipBlock(), std::exception);
  
} // TestFileTest()


//------------------------------------------------------------------------------
void BlockElementTest() {
  
  using namespace util::fblock;
  
  std::vector<double> const expectedData = makeTestData<tf::Data>();
  
  std::ifstream srcFile { TestFilePath, std::ios::in };
  srcFile.exceptions
    (std::ifstream::failbit | std::ifstream::eofbit | std::ifstream::badbit);
  
  std::ofstream destFile
    { "incr_" + TestFilePath, std::ios::out | std::ios::trunc };
  destFile.exceptions
    (std::ifstream::failbit | std::ifstream::eofbit | std::ifstream::badbit);
  
  bool res;
  
  //
  // Version
  //
  Version version;
  BOOST_CHECK(version.hasKey("VERS"));
  BOOST_CHECK_EQUAL(version(), 0);
  res = version.read(srcFile);
  BOOST_CHECK(res);
  
  BOOST_CHECK(version.hasKey(getKey<tf::Version>()));
  BOOST_CHECK_EQUAL(version.key(), MagicKey{ getKey<tf::Version>() });
  
  BOOST_CHECK_EQUAL(version.version(), getData<tf::Version>());
  BOOST_CHECK_EQUAL(version(), getData<tf::Version>());
  
  Version versionCopy { version };
  BOOST_CHECK(versionCopy.hasKey(version.key()));
  BOOST_CHECK_EQUAL(versionCopy(), version());
  versionCopy.set(version() + 1);
  BOOST_CHECK(versionCopy.hasKey(version.key()));
  BOOST_CHECK_EQUAL(versionCopy(), version() + 1);
  
  res = versionCopy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(versionCopy.hasKey(version.key()));
  BOOST_CHECK_EQUAL(versionCopy(), version() + 1);
  
  //
  // String
  //
  String string1;
  BOOST_CHECK(string1.hasKey(NullKey));
  BOOST_CHECK_EQUAL(string1(), "");
  res = string1.read(srcFile);
  BOOST_CHECK(res);
  
  BOOST_CHECK(string1.hasKey(getKey<tf::String1>()));
  BOOST_CHECK_EQUAL(string1.key(), MagicKey{ getKey<tf::String1>() });
  
  BOOST_CHECK_EQUAL(std::string_view(string1), getData<tf::String1>());
  BOOST_CHECK_EQUAL(std::string(string1), getData<tf::String1>());
  BOOST_CHECK_EQUAL(string1(), getData<tf::String1>());
  
  String string1copy { string1 };
  BOOST_CHECK(string1copy.hasKey(string1.key()));
  BOOST_CHECK_EQUAL(string1copy(), string1());
  string1copy.set(std::string(string1) + "1"s);
  BOOST_CHECK(string1copy.hasKey(string1.key()));
  BOOST_CHECK_EQUAL(string1copy(), std::string(string1) + "1"s);

  res = string1copy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(string1copy.hasKey(string1.key()));
  BOOST_CHECK_EQUAL(string1copy(), std::string(string1) + "1"s);
  
  //
  // Number<unsigned int>
  //
  Number<unsigned int> number1;
  BOOST_CHECK(number1.hasKey(NullKey));
  BOOST_CHECK_EQUAL(number1.value(), 0U);
  res = number1.read(srcFile);
  BOOST_CHECK(res);
  
  BOOST_CHECK(number1.hasKey(getKey<tf::Num1>()));
  BOOST_CHECK_EQUAL(number1.key(), MagicKey{ getKey<tf::Num1>() });
  
  BOOST_CHECK_EQUAL(number1.value(), getData<tf::Num1>());
  BOOST_CHECK_EQUAL(number1(), getData<tf::Num1>());
  
  Number<unsigned int> number1copy { number1 };
  BOOST_CHECK(number1copy.hasKey(number1.key()));
  BOOST_CHECK_EQUAL(number1copy(), number1());
  number1copy.set(number1() + 1);
  BOOST_CHECK(number1copy.hasKey(number1.key()));
  BOOST_CHECK_EQUAL(number1copy(), number1() + 1);

  res = number1copy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(number1copy.hasKey(number1.key()));
  BOOST_CHECK_EQUAL(number1copy(), number1() + 1);
  
  //
  // Number<long long int>
  //
  Number<long long int> number2;
  BOOST_CHECK(number2.hasKey(NullKey));
  BOOST_CHECK_EQUAL(number2.value(), 0LL);
  res = number2.read(srcFile);
  BOOST_CHECK(res);
  
  BOOST_CHECK(number2.hasKey(getKey<tf::Num2>()));
  BOOST_CHECK_EQUAL(number2.key(), MagicKey{ getKey<tf::Num2>() });
  
  BOOST_CHECK_EQUAL(number2.value(), getData<tf::Num2>());
  BOOST_CHECK_EQUAL(number2(), getData<tf::Num2>());
  
  Number<long long int> number2copy { number2 };
  BOOST_CHECK(number2copy.hasKey(number2.key()));
  BOOST_CHECK_EQUAL(number2copy(), number2());
  number2copy.set(number2() + 1);
  BOOST_CHECK(number2copy.hasKey(number2.key()));
  BOOST_CHECK_EQUAL(number2copy(), number2() + 1);

  res = number2copy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(number2copy.hasKey(number2.key()));
  BOOST_CHECK_EQUAL(number2copy(), number2() + 1);
  
  // 
  // Bookmark
  //
  Bookmark mark1;
  BOOST_CHECK(mark1.hasKey(NullKey));
  BOOST_CHECK_EQUAL(mark1.size(), 0U);
  res = mark1.read(srcFile);
  BOOST_CHECK(res);
  BOOST_CHECK_EQUAL(mark1.size(), 0U);
  
  BOOST_CHECK(mark1.hasKey(getKey<tf::Mark1>()));
  BOOST_CHECK_EQUAL(mark1.key(), MagicKey{ getKey<tf::Mark1>() });
  
  Bookmark mark1copy { mark1 };
  BOOST_CHECK(mark1copy.hasKey(mark1.key()));

  res = mark1copy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(mark1copy.hasKey(mark1.key()));
  
  //
  // FileBlock
  //
  FileBlock data;
  BOOST_CHECK(data.hasKey(NullKey));
  BOOST_CHECK_EQUAL(data.size(), 0U);
  res = data.read(srcFile);
  BOOST_CHECK(res);
  
  BOOST_CHECK(data.hasKey(getKey<tf::Data>()));
  BOOST_CHECK_EQUAL(data.key(), MagicKey{ getKey<tf::Data>() });
  
  BOOST_CHECK_EQUAL(data.size(), getData<tf::Data>() * sizeof(double));
  auto const& dataColl = data.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataColl.begin(), dataColl.end(),
    expectedData.begin(), expectedData.end()
    );
  
  BOOST_CHECK_EQUAL(data.sizeAs<double>(), getData<tf::Data>());
  std::size_t alignedSize = data.size();
  if (alignedSize % WordSize != 0)
    alignedSize += WordSize - alignedSize % WordSize;
  BOOST_CHECK_EQUAL(data.alignedSize(), alignedSize);
  BOOST_CHECK_EQUAL
    (data.paddingSize(), alignedSize - getData<tf::Data>() * sizeof(double));
  double const* bufferPtr = data.payload<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    bufferPtr, bufferPtr + data.sizeAs<double>(),
    expectedData.begin(), expectedData.end()
    );
  BOOST_CHECK_EQUAL(
    reinterpret_cast<void const*>(data.payloadBuffer()),
    reinterpret_cast<void const*>(bufferPtr)
    );
  BOOST_CHECK_EQUAL(&(data.payloadAs<double>()), bufferPtr);
  
  FileBlock dataCopy { data };
  BOOST_CHECK(dataCopy.hasKey(data.key()));
  BOOST_CHECK_EQUAL(dataCopy.size(), data.size());
  auto dataCopyColl = dataCopy.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataCopyColl.begin(), dataCopyColl.end(),
    dataColl.begin(), dataColl.end()
    );

  auto expectedDataCopy { expectedData };
  expectedDataCopy.push_back(1.0);
  dataCopy.setPayload
    (expectedDataCopy.size() * sizeof(double), expectedDataCopy.data());
  BOOST_CHECK(dataCopy.hasKey(data.key()));
  BOOST_CHECK_EQUAL(dataCopy.size(), expectedDataCopy.size() * sizeof(double));
  dataCopyColl = dataCopy.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataCopyColl.begin(), dataCopyColl.end(),
    expectedDataCopy.cbegin(), expectedDataCopy.cend()
    );

  res = dataCopy.write(destFile);
  BOOST_CHECK(res);
  BOOST_CHECK(dataCopy.hasKey(data.key()));
  BOOST_CHECK_EQUAL(dataCopy.size(), expectedDataCopy.size() * sizeof(double));
  dataCopyColl = dataCopy.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataCopyColl.begin(), dataCopyColl.end(),
    expectedDataCopy.cbegin(), expectedDataCopy.cend()
    );
  
  //
  // String (non-essential checks are not repeated)
  //
  String string2;
  res = string2.read(srcFile);
  BOOST_CHECK(res);
  BOOST_CHECK(string2.hasKey(getKey<tf::String2>()));
  string2.set(std::string(string2) + "1"s);
  res = string2.write(destFile);
  BOOST_CHECK(res);
  
  // 
  // Bookmark (non-essential checks are not repeated)
  //
  Bookmark mark2;
  res = mark2.read(srcFile);
  BOOST_CHECK(res);
  res = mark2.write(destFile);
  BOOST_CHECK(res);
  
  //
  // past the end
  //
  
  // switch off exceptions, or else the read failure might be caught by the
  // ifstream object
  srcFile.exceptions(std::ifstream::goodbit);
  
  String pastEnd { MagicKey{ "TEST" }, "NotEmptyAtAll"s };
  BOOST_CHECK(pastEnd.hasKey("TEST"));
  BOOST_CHECK_EQUAL(pastEnd.size(), "NotEmptyAtAll"s.size());
  res = pastEnd.read(srcFile);
  BOOST_CHECK(!res);
  BOOST_CHECK(pastEnd.hasKey(NullKey));
  BOOST_CHECK_EQUAL(pastEnd.size(), 0U);
  
} // BlockElementTest()


//------------------------------------------------------------------------------
void UpdatedReadTest() {
  //
  // minimal test to check that the content of the file has been written
  // correctly; all values have been "increased" by 1 (except the bookmarks),
  // and the keys are the same
  //
  using namespace util::fblock;
  
  std::vector<double> expectedData = makeTestData<tf::Data>();
  expectedData.push_back(1);
  
  util::BinaryBlockFile srcFile({ "incr_"s + TestFilePath, std::ios::in });
  
  auto const& version = srcFile.readVersion(getKey<tf::Version>());
  BOOST_CHECK_EQUAL(version.version(), getData<tf::Version>() + 1);
  
  auto const& string1 = srcFile.readBlock<String>(getKey<tf::String1>());
  BOOST_CHECK_EQUAL(std::string(string1), getData<tf::String1>() + "1"s);
  
  auto const& number1
    = srcFile.readBlock<Number<unsigned int>>(getKey<tf::Num1>());
  BOOST_CHECK_EQUAL(number1.value(), getData<tf::Num1>() + 1);
  
  auto const& number2
    = srcFile.readBlock<Number<long long int>>(getKey<tf::Num2>());
  BOOST_CHECK_EQUAL(number2.value(), getData<tf::Num2>() + 1);
  
  srcFile.skipBlock(getKey<tf::Mark1>(), "bookmark 1"_bd);
  
  auto const& data
    = srcFile.readBlock(getKey<tf::Data>(), "data"_bd);
  BOOST_CHECK_EQUAL(data.size(), expectedData.size() * sizeof(double));
  auto const& dataColl = data.payloadSequence<double>();
  BOOST_CHECK_EQUAL_COLLECTIONS(
    dataColl.begin(), dataColl.end(),
    expectedData.begin(), expectedData.end()
    );
  
  auto const& string2 = srcFile.readBlock<String>(getKey<tf::String2>());
  BOOST_CHECK_EQUAL(std::string(string2), getData<tf::String2>() + "1"s);
  
  srcFile.skipBlock(getKey<tf::EndMark>(), "end"_bd);
  
  BOOST_CHECK_THROW(srcFile.skipBlock(), std::exception);
  
} // UpdatedReadTest()


//------------------------------------------------------------------------------

BOOST_AUTO_TEST_CASE(WriteRead_TestCase) {
  
  WriterTest();
  TestFileTest();
  BlockElementTest();
  UpdatedReadTest();
  
} // BOOST_AUTO_TEST_CASE(WriteRead_TestCase)

//------------------------------------------------------------------------------
