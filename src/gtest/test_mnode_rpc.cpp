#include "base58.h"

#include <gtest/gtest.h>

#include <tuple>

#include "mnode/mnode-rpc.h"
#include "utilstrencodings.h"

using namespace testing;
using namespace std;

string Base58Encode_TestKey(const string& s)
{
    vector<unsigned char> vch(s.cbegin(), s.cend());
    return EncodeBase58Check(vch);
}

class PTest_ani2psl_secret : public TestWithParam<tuple<
        string (*)(), // generate private key string
        bool,  // CKey::IsValid()
        bool   // CKey::IsCompressed()
    >>        
{
public:
    static void SetUpTestCase()
    {
        SelectParams(CBaseChainParams::Network::REGTEST);
    }
};



TEST_P(PTest_ani2psl_secret, test)
{
    const string sKey = get<0>(GetParam())();
    const bool bResult = get<1>(GetParam());
    const bool bCompressed = get<2>(GetParam());
    string sKeyError;

    const CKey key = ani2psl_secret(sKey, sKeyError);
    EXPECT_EQ(bResult, key.IsValid()) << "KeyError: " << sKeyError;
    EXPECT_EQ(bResult, sKeyError.empty());

    // check SECP256K1_EC_COMPRESSED flag
    EXPECT_EQ(bCompressed, key.IsCompressed());
}

constexpr auto TestValidKey = "private key is base58 encoded___";

INSTANTIATE_TEST_SUITE_P(mnode_rpc_ani2psl, PTest_ani2psl_secret,
	Values(
		// key not base58 encoded
		make_tuple([]() -> string { return "test";}, 
            false, false),
        // base58 encoding, but key is too short
        make_tuple([]()
        {
            return Base58Encode_TestKey("test private key");
        }, false, false),
        // correct size, but no prefix
        make_tuple([]()
        {
            return Base58Encode_TestKey(TestValidKey);
        }, false, false),
        // invalid prefix (SECRET_KEY) for the current network (regtest)
        make_tuple([]()
        {
            const auto& v = Params().Base58Prefix(CChainParams::Base58Type::SECRET_KEY);
            string s(v.size(), 'a');
            s += TestValidKey;
            return Base58Encode_TestKey(s);
        }, false, false),
        // valid private key - compressed flag is off
        make_tuple([]()
        {
            const auto& v = Params().Base58Prefix(CChainParams::Base58Type::SECRET_KEY);
            string s(v.cbegin(), v.cend());
            s += TestValidKey;
            return Base58Encode_TestKey(s);
        }, true, false),
        // valid private key - compressed flag is on
        make_tuple([]()
        {
            const auto& v = Params().Base58Prefix(CChainParams::Base58Type::SECRET_KEY);
            string s(v.cbegin(), v.cend());
            s += TestValidKey;
            s += '\1';
            return Base58Encode_TestKey(s);
        }, true, true)
	));

class PTest_ASCII85_Encode_Decode: public TestWithParam<tuple<string, string>>       
{
public:
    static void SetUpTestCase()
    {
        SelectParams(CBaseChainParams::Network::REGTEST);
    }
};


TEST_P(PTest_ASCII85_Encode_Decode, ASCII85_Encode_Decode) {

    const string to_be_encoded_1 = get<0>(GetParam()); //"hello"; // Encoded shall be : "BOu!rDZ"
    const string to_be_decoded_1 = get<1>(GetParam()); //"BOu!rDZ"; // Decoded shall be: "hello"
    string encoded = EncodeAscii85(to_be_encoded_1);
    string decoded = DecodeAscii85(to_be_decoded_1);

    EXPECT_EQ(to_be_encoded_1, decoded);
    EXPECT_EQ(to_be_decoded_1, encoded);
}

INSTANTIATE_TEST_SUITE_P(encode_decode_tests, PTest_ASCII85_Encode_Decode,
	Values(
		make_tuple("hello","BOu!rDZ"),
		make_tuple("how are you","BQ&);@<,p%H#Ig"),
		make_tuple("0x56307893281ndjnskdndsfhdsufiolm","0R,H51GCaI3AWEM0lCN:DKBT(DIdg#BOl1,Anc1\"D#")
	));
