// Copyright (c) 2018 The PASTELCoin Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "key_io.h"
#include "core_io.h"
#include "deprecation.h"
#include "script/sign.h"
#include "init.h"

#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif // ENABLE_WALLET

#include "mnode-controller.h"
#include "mnode-pastel.h"

#include "ed448/pastel_key.h"
#include "json/json.hpp"

#include <algorithm>

using json = nlohmann::json;

void CPastelTicketProcessor::InitTicketDB()
{
	boost::filesystem::path ticketsDir = GetDataDir() / "tickets";
	if (!boost::filesystem::exists(ticketsDir)) {
		boost::filesystem::create_directories(ticketsDir);
	}
	
	uint64_t nTotalCache = (GetArg("-dbcache", 450) << 20);
	uint64_t nMinDbCache = 4, nMaxDbCache = 16384; //16KB
	nTotalCache = std::max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
	nTotalCache = std::min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greated than nMaxDbcache
	uint64_t nTicketDBCache = nTotalCache / 8 / uint8_t(TicketID::COUNT);
	
	dbs[TicketID::PastelID] = std::unique_ptr<CDBWrapper>(new CDBWrapper(GetDataDir() / "tickets" / "pslids", nTicketDBCache, false, fReindex));
	dbs[TicketID::Art] 		= std::unique_ptr<CDBWrapper>(new CDBWrapper(GetDataDir() / "tickets" / "artreg", nTicketDBCache, false, fReindex));
	dbs[TicketID::Activate] 	= std::unique_ptr<CDBWrapper>(new CDBWrapper(GetDataDir() / "tickets" / "artcnf", nTicketDBCache, false, fReindex));
	dbs[TicketID::Trade] 	= std::unique_ptr<CDBWrapper>(new CDBWrapper(GetDataDir() / "tickets" / "arttrd", nTicketDBCache, false, fReindex));
	dbs[TicketID::Down] 	= std::unique_ptr<CDBWrapper>(new CDBWrapper(GetDataDir() / "tickets" / "takedn", nTicketDBCache, false, fReindex));
}

void CPastelTicketProcessor::UpdatedBlockTip(const CBlockIndex *pindex, bool fInitialDownload)
{
	if(!pindex) return;
	
	if (fInitialDownload){
		//??
	}
	
	CBlock block;
	if(!ReadBlockFromDisk(block, pindex)) {
		LogPrintf("CPastelTicket::UpdatedBlockTip -- ERROR: Can't read block from disk\n");
		return;
	}

	for(const CTransaction& tx : block.vtx)
	{
		CMutableTransaction mtx(tx);
		ParseTicketAndUpdateDB(mtx, pindex->nHeight);
	}
}

template<class T>
bool CPastelTicketProcessor::UpdateDB(T& ticket, std::string txid, int nBlockHeight)
{
	if (!txid.empty()) ticket.ticketTnx = std::move(txid);
	if (nBlockHeight != 0) ticket.ticketBlock = nBlockHeight;
	dbs[ticket.ID()]->Write(ticket.KeyOne(), ticket);
	dbs[ticket.ID()]->Write(ticket.KeyTwo(), ticket.KeyOne());
	//LogPrintf("tickets", "CPastelTicketProcessor::UpdateDB -- Ticket added into DB with key %s (txid - %s)\n", ticket.KeyOne(), ticket.ticketTnx);
	return true;
}

bool preParseTicket(const CMutableTransaction& tx, std::vector<unsigned char>& data, TicketID& ticket_id, std::string& error)
{
	if (!CPastelTicketProcessor::ParseP2FMSTransaction(tx, data, error)){
		return false;
	}
	
	auto ticket_id_byte = data;
	auto ticket_id_ptr = reinterpret_cast<TicketID **>(&ticket_id_byte);
	if (ticket_id_ptr == nullptr || *ticket_id_ptr == nullptr) {
		LogPrintf("CPastelTicketProcessor::ParseTicketAndUpdateDB -- ERROR: Failed to parse and unpack ticket - wrong ticket_id (txid - %s)\n", tx.GetHash().GetHex());
		return false;
	}
	
	ticket_id = **ticket_id_ptr;
	
	return true;
}

bool CPastelTicketProcessor::ParseTicketAndUpdateDB(CMutableTransaction& tx, int nBlockHeight)
{
	std::string error;
	std::vector<unsigned char> data;
	TicketID ticket_id;
	
	if (!preParseTicket(tx, data, ticket_id, error))
		return false;
	
	try {
		std::string txid = tx.GetHash().GetHex();
		
		if (ticket_id == TicketID::PastelID) {
			auto ticket = ParseTicket<CPastelIDRegTicket>(data, sizeof(TicketID));
			return UpdateDB<CPastelIDRegTicket>(ticket, txid, nBlockHeight);
		}
		if (ticket_id == TicketID::Art) {
			auto ticket = ParseTicket<CArtRegTicket>(data, sizeof(TicketID));
			return UpdateDB<CArtRegTicket>(ticket, txid, nBlockHeight);
		}
//		if (ticket_id == TicketID::Confirm) {
//			auto ticket = ParseTicket<CArtActivateTicket>(data, sizeof(TicketID));
//			return UpdateDB<CArtActivateTicket>(ticket, txid, nBlockHeight);
//		}
//		if (ticket_id == TicketID::Trade) {
//			auto ticket = ParseTicket<CArtTradeTicket>(data, sizeof(TicketID));
//			return UpdateDB<CArtTradeTicket>(ticket, txid, nBlockHeight);
//		}
//		if (ticket_id == TicketID::Down) {
//			auto ticket = ParseTicket<CTakeDownTicket>(data, sizeof(TicketID));
//			return UpdateDB<CTakeDownTicket>(ticket, txid, nBlockHeight);
//		}
	}catch (...)
	{
		LogPrintf("CPastelTicketProcessor::ParseTicketAndUpdateDB -- ERROR: Failed to parse and unpack ticket with ticket_id %d from txid - %s\n", (int)ticket_id, tx.GetHash().GetHex());
	}
	return false;
}

std::string CPastelTicketProcessor::GetTicketJSON(uint256 txid)
{
	auto ticket = GetTicket(txid);
	if (ticket != nullptr)
		return ticket->ToJSON();
	else
		return "";
}

CPastelTicketBase* CPastelTicketProcessor::GetTicket(uint256 txid)
{
	CTransaction tx;
	uint256 hashBlock;
	if (!GetTransaction(txid, tx, hashBlock, true))
		throw std::runtime_error(strprintf("No information available about transaction"));

	CMutableTransaction mtx(tx);
	
	std::string error;
	std::vector<unsigned char> data;
	TicketID ticket_id;
	
	if (!preParseTicket(mtx, data, ticket_id, error))
		throw std::runtime_error(strprintf("Failed to create P2FMS from data provided - %s", error));
	
	CPastelTicketBase* ticket = nullptr;
	
	try {
		std::string str_txid = tx.GetHash().GetHex();
		int height = -1;
		if (mapBlockIndex.count(hashBlock) != 0)
			height = mapBlockIndex[hashBlock]->nHeight;
		
		if (ticket_id == TicketID::PastelID) {
			ticket = new CPastelIDRegTicket{ParseTicket<CPastelIDRegTicket>(data, sizeof(TicketID))};
		}
		if (ticket_id == TicketID::Art) {
			ticket = new CArtRegTicket{ParseTicket<CArtRegTicket>(data, sizeof(TicketID))};
		}
//		if (ticket_id == TicketID::Confirm) {
//			auto ticket = ParseTicket<CArtActivateTicket>(data, sizeof(TicketID));
//		}
//		if (ticket_id == TicketID::Trade) {
//			auto ticket = ParseTicket<CArtTradeTicket>(data, sizeof(TicketID));
//		}
//		if (ticket_id == TicketID::Down) {
//			auto ticket = ParseTicket<CTakeDownTicket>(data, sizeof(TicketID));
//		}
		
		if (ticket != nullptr) {
			ticket->ticketTnx = std::move(str_txid);
			ticket->ticketBlock = height;
		}

	}catch (...)
	{
		LogPrintf("CPastelTicketProcessor::GetTicket -- ERROR: Failed to parse and unpack ticket with ticket_id %d from txid - %s\n", (int)ticket_id, tx.GetHash().GetHex());
	}
	
	return ticket;
}

template<class T>
bool CPastelTicketProcessor::CheckTicketExist(const T& ticket)
{
	auto key = ticket.KeyOne();
	return dbs[ticket.ID()]->Exists(key);
}
template bool CPastelTicketProcessor::CheckTicketExist<CPastelIDRegTicket>(const CPastelIDRegTicket&);
template bool CPastelTicketProcessor::CheckTicketExist<CArtRegTicket>(const CArtRegTicket&);
template bool CPastelTicketProcessor::CheckTicketExist<CArtActivateTicket>(const CArtActivateTicket&);

template<class T>
bool CPastelTicketProcessor::CheckTicketExistBySecondaryKey(const T& ticket)
{
	decltype(ticket.KeyOne()) mainKey;
	if (dbs[ticket.ID()]->Read(ticket.KeyTwo(), mainKey))
		return dbs[ticket.ID()]->Exists(mainKey);
	return false;
}
template bool CPastelTicketProcessor::CheckTicketExistBySecondaryKey<CPastelIDRegTicket>(const CPastelIDRegTicket&);
template bool CPastelTicketProcessor::CheckTicketExistBySecondaryKey<CArtRegTicket>(const CArtRegTicket&);
template bool CPastelTicketProcessor::CheckTicketExistBySecondaryKey<CArtActivateTicket>(const CArtActivateTicket&);

template<class T>
bool CPastelTicketProcessor::FindTicket(T& ticket)
{
	auto key = ticket.KeyOne();
	return dbs[ticket.ID()]->Read(key, ticket);
}
template bool CPastelTicketProcessor::FindTicket<CPastelIDRegTicket>(CPastelIDRegTicket&);
template bool CPastelTicketProcessor::FindTicket<CArtRegTicket>(CArtRegTicket&);
template bool CPastelTicketProcessor::FindTicket<CArtActivateTicket>(CArtActivateTicket&);

template<class T>
bool CPastelTicketProcessor::FindTicketBySecondaryKey(T& ticket)
{
	decltype(ticket.KeyOne()) mainKey;
	if (dbs[ticket.ID()]->Read(ticket.KeyTwo(), mainKey))
		return dbs[ticket.ID()]->Read(mainKey, ticket);
	return false;
}
template bool CPastelTicketProcessor::FindTicketBySecondaryKey<CPastelIDRegTicket>(CPastelIDRegTicket&);
template bool CPastelTicketProcessor::FindTicketBySecondaryKey<CArtRegTicket>(CArtRegTicket&);
template bool CPastelTicketProcessor::FindTicketBySecondaryKey<CArtActivateTicket>(CArtActivateTicket&);

std::vector<std::string> CPastelTicketProcessor::GetAllKeys(TicketID id)
{
	std::vector<std::string> results;
	
	std::unique_ptr<CDBIterator> pcursor(dbs[id]->NewIterator());
	pcursor->SeekToFirst();
	while (pcursor->Valid()) {
		std::string key;
		if (pcursor->GetKey(key)) {
			results.emplace_back(key);
		}
		pcursor->Next();
	}
	return results;
}

template<class T>
std::string CPastelTicketProcessor::SendTicket(const T& ticket)
{
    std::string error;
    if (!ticket.IsValid(true, error)){
        throw std::runtime_error(strprintf("Ticket (%s) is invalid - %s", ticket.TicketName(), error));
    }
    
	msgpack::sbuffer buffer;
	msgpack::pack(buffer, ticket);
	
	auto pdata = reinterpret_cast<unsigned char*>(buffer.data());
	std::vector<unsigned char> data{pdata, pdata+buffer.size()};
	
	TicketID tid = ticket.ID();
	auto ticketid_byte = reinterpret_cast<unsigned char*>(&tid);
	data.insert(data.begin(), ticketid_byte, ticketid_byte+sizeof(TicketID)); //sizeof(size_t) == 8
	
	CMutableTransaction tx;
	if (!CPastelTicketProcessor::CreateP2FMSTransaction(data, tx, GetTicketPrice(tid), error)){
		throw std::runtime_error(strprintf("Failed to create P2FMS from data provided - %s", error));
	}
	
	if (!CPastelTicketProcessor::StoreP2FMSTransaction(tx, error)){
		throw std::runtime_error(strprintf("Failed to send P2FMS transaction - %s", error));
	}
	return tx.GetHash().GetHex();
}
template std::string CPastelTicketProcessor::SendTicket<CPastelIDRegTicket>(const CPastelIDRegTicket&);
template std::string CPastelTicketProcessor::SendTicket<CArtRegTicket>(const CArtRegTicket&);
template std::string CPastelTicketProcessor::SendTicket<CArtActivateTicket>(const CArtActivateTicket&);

template<class T>
T CPastelTicketProcessor::ParseTicket(const std::vector<unsigned char>& data, int nOffset)
{
	T t;
	auto pdata = const_cast<char*>(reinterpret_cast<const char*>(data.data()));
	msgpack::object_handle oh = msgpack::unpack(pdata+nOffset, data.size()-1);
	msgpack::object obj = oh.get();
	obj.convert(t);
	return t;
}
template CPastelIDRegTicket CPastelTicketProcessor::ParseTicket<CPastelIDRegTicket>(const std::vector<unsigned char>&, int);
template CArtRegTicket CPastelTicketProcessor::ParseTicket<CArtRegTicket>(const std::vector<unsigned char>&, int);
template CArtActivateTicket CPastelTicketProcessor::ParseTicket<CArtActivateTicket>(const std::vector<unsigned char>&, int);

#ifdef ENABLE_WALLET
bool CPastelTicketProcessor::CreateP2FMSTransaction(const std::string& input_data, CMutableTransaction& tx_out, CAmount price, std::string& error_ret)
{
    //Convert string data into binary buffer
    std::vector<unsigned char> input_bytes = ToByteVector(input_data);
    return CPastelTicketProcessor::CreateP2FMSTransaction(input_bytes, tx_out, price, error_ret);
}

bool CPastelTicketProcessor::CreateP2FMSTransaction(const std::vector<unsigned char>& input_data, CMutableTransaction& tx_out, CAmount price, std::string& error_ret)
{
	assert(pwalletMain != nullptr);
	
	if (pwalletMain->IsLocked()) {
		error_ret = "Wallet is locked. Try again later";
		return false;
	}
	
    size_t input_len = input_data.size();
    if (input_len == 0) {
        error_ret = "Input data is empty";
        return false;
    }

    std::vector<unsigned char> input_bytes = input_data;

    //Get Hash(SHA256) of input buffer and insert it upfront
    uint256 input_hash = Hash(input_bytes.begin(), input_bytes.end());
    input_bytes.insert(input_bytes.begin(), input_hash.begin(), input_hash.end());

    //insert size of the original data upfront
    auto* input_len_bytes = reinterpret_cast<unsigned char*>(&input_len);
    input_bytes.insert(input_bytes.begin(), input_len_bytes, input_len_bytes+sizeof(size_t)); //sizeof(size_t) == 8

    //Add padding at the end if required -
    // final size is n*33 - (33 bytes, but 66 characters)
    int fake_key_size = 33;
    size_t non_padded_size = input_bytes.size();
    size_t padding_size = fake_key_size - (non_padded_size % fake_key_size);
    if (padding_size != 0){
        input_bytes.insert(input_bytes.end(), padding_size, 0);
    }

    //Break data into 33 bytes blocks
    std::vector<std::vector<unsigned char> > chunks;
    for (auto it = input_bytes.begin(); it != input_bytes.end(); it += fake_key_size){
        chunks.emplace_back(std::vector<unsigned char>(it, it+fake_key_size));
    }

    //Create output P2FMS scripts
    std::vector<CScript> out_scripts;
    for (auto it=chunks.begin(); it != chunks.end(); ) {
        CScript script;
        script << CScript::EncodeOP_N(1);
        int m=0;
        for (; m<3 && it != chunks.end(); m++, it++) {
            script << *it;
        }
        script << CScript::EncodeOP_N(m) << OP_CHECKMULTISIG;
        out_scripts.push_back(script);
    }
    int num_fake_txn = out_scripts.size();
    if (num_fake_txn == 0){
        error_ret = "No fake transactions after parsing input data";
        return false;
    }

    //Create address and script for change
    CKey key_change;
    key_change.MakeNewKey(true);
    CScript script_change;
    script_change = GetScriptForDestination(key_change.GetPubKey().GetID());

    //calculate aprox required amount
    CAmount nAproxFeeNeeded = payTxFee.GetFee(input_bytes.size())*2;
    if (nAproxFeeNeeded < payTxFee.GetFeePerK()) nAproxFeeNeeded = payTxFee.GetFeePerK();
    
    //Amount
	CAmount perOutputAmount = price*COIN/out_scripts.size();
	//MUST be precise!!!
	CAmount lost = price*COIN - perOutputAmount*out_scripts.size();
			
    CAmount allSpentAmount = price*COIN + nAproxFeeNeeded;

    int chainHeight = chainActive.Height() + 1;
    if (Params().NetworkIDString() != "regtest") {
        chainHeight = std::max(chainHeight, APPROX_RELEASE_HEIGHT);
    }
    auto consensusBranchId = CurrentEpochBranchId(chainHeight, Params().GetConsensus());

    //Create empty transaction
    tx_out = CreateNewContextualCMutableTransaction(Params().GetConsensus(), chainHeight);

    //Find funding (unspent) transaction with enough coins to cover all outputs (single - for simplicity)
    bool bOk = false;
    {
        vector<COutput> vecOutputs;
        LOCK(pwalletMain->cs_wallet);
        pwalletMain->AvailableCoins(vecOutputs, false, nullptr, true);
        for (auto out : vecOutputs) {
            if (out.tx->vout[out.i].nValue > allSpentAmount) {

                //If found - populate transaction

                const CScript& prevPubKey = out.tx->vout[out.i].scriptPubKey;
                const CAmount& prevAmount = out.tx->vout[out.i].nValue;

                tx_out.vin.resize(1);
                tx_out.vin[0].prevout.n = out.i;
                tx_out.vin[0].prevout.hash = out.tx->GetHash();

                //Add fake output scripts
                tx_out.vout.resize(num_fake_txn + 1); //+1 for change
                for (int i=0; i<num_fake_txn; i++) {
                    tx_out.vout[i].nValue = perOutputAmount;
                    tx_out.vout[i].scriptPubKey = out_scripts[i];
                }
                //MUST be precise!!!
                tx_out.vout[0].nValue = perOutputAmount + lost;

				//Add change output scripts
				tx_out.vout[num_fake_txn].nValue = prevAmount - price*COIN;
				tx_out.vout[num_fake_txn].scriptPubKey = script_change;

                //sign transaction - unlock input
                SignatureData sigdata;
                ProduceSignature(MutableTransactionSignatureCreator(pwalletMain, &tx_out, 0, prevAmount, SIGHASH_ALL), prevPubKey, sigdata, consensusBranchId);
                UpdateTransaction(tx_out, 0, sigdata);

                //Calculate correct fee
                size_t tx_size = EncodeHexTx(tx_out).length();
                CAmount nFeeNeeded = payTxFee.GetFee(tx_size);
                if (nFeeNeeded < payTxFee.GetFeePerK()) nFeeNeeded = payTxFee.GetFeePerK();

                //num_fake_txn is index of the change output
                tx_out.vout[num_fake_txn].nValue -= nFeeNeeded;

                bOk = true;
                break;
            }
        }
    }

    if (!bOk){
        error_ret = "No unspent transaction found - cannot send data to the blockchain!";
    }
    return bOk;
}
#endif // ENABLE_WALLET

bool CPastelTicketProcessor::StoreP2FMSTransaction(const CMutableTransaction& tx_out, std::string& error_ret)
{
	CValidationState state;
	bool fMissingInputs;
	if (!AcceptToMemoryPool(mempool, state, tx_out, false, &fMissingInputs, true)) {
		if (state.IsInvalid()) {
			error_ret = strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason());
			return false;
		} else {
			if (fMissingInputs) {
				error_ret = "Missing inputs";
				return false;
			}
			error_ret = state.GetRejectReason();
			return false;
		}
	}
	
	RelayTransaction(tx_out);
	return true;
}
bool CPastelTicketProcessor::ParseP2FMSTransaction(const CMutableTransaction& tx_in, std::string& output_data, std::string& error_ret)
{
	std::vector<unsigned char> output_vector;
	bool bOk = CPastelTicketProcessor::ParseP2FMSTransaction(tx_in, output_vector, error_ret);
	if (bOk)
		output_data.assign(output_vector.begin(), output_vector.end());
	return bOk;
}
bool CPastelTicketProcessor::ParseP2FMSTransaction(const CMutableTransaction& tx_in, std::vector<unsigned char>& output_data, std::string& error_ret)
{
	bool foundMS = false;
	
	for (const auto& vout : tx_in.vout) {
		
		txnouttype typeRet;
		vector<vector<unsigned char> > vSolutions;
		
		if (!Solver(vout.scriptPubKey, typeRet, vSolutions) ||
			typeRet != TX_MULTISIG)
			continue;
		
		foundMS = true;
		for (size_t i = 1; vSolutions.size() - 1 > i; i++)
		{
			output_data.insert(output_data.end(), vSolutions[i].begin(), vSolutions[i].end());
		}
	}
	
	if (!foundMS){
		error_ret = "No data Multisigs found in transaction";
		return false;
	}
	
	if (output_data.empty()){
		error_ret = "No data found in transaction";
		return false;
	}
	
	//size_t szie = 8 bytes; hash size = 32 bytes
	if (output_data.size() < 40){
		error_ret = "No correct data found in transaction";
		return false;
	}

//    std::vector<unsigned char> output_len_bytes(output_data.begin(), output_data.begin()+sizeof(size_t)); //sizeof(size_t) == 8
	auto output_len_ptr = reinterpret_cast<size_t**>(&output_data);
	if (output_len_ptr == nullptr || *output_len_ptr == nullptr){
		error_ret = "No correct data found in transaction - wrong length";
		return false;
	}
	auto output_len = **output_len_ptr;
	output_data.erase(output_data.begin(), output_data.begin()+sizeof(size_t));
	
	std::vector<unsigned char> input_hash_vec(output_data.begin(), output_data.begin()+32); //hash length == 32
	output_data.erase(output_data.begin(), output_data.begin()+32);
	
	if (output_data.size() < output_len){
		error_ret = "No correct data found in transaction - length is not matching";
		return false;
	}
	
	if (output_data.size() > output_len) {
		output_data.erase(output_data.begin()+output_len, output_data.end());
	}
	
	uint256 input_hash_stored(input_hash_vec);
	uint256 input_hash_real = Hash(output_data.begin(), output_data.end());
	
	if (input_hash_stored != input_hash_real) {
		error_ret = "No correct data found in transaction - hash is not matching";
		return false;
	}
	
	return true;
}

CAmount CPastelTicketProcessor::GetTicketPrice(TicketID tid)
{
	switch(tid)
	{
		case TicketID::PastelID: 	return 10;
		case TicketID::Art: 		return 0;
		case TicketID::Activate: 	return 0;
		case TicketID::Trade: 		return 1000;
		case TicketID::Down: 		return 1000;
	};
	return 0;
}

// CPastelIDRegTicket ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*static*/ CPastelIDRegTicket CPastelIDRegTicket::Create(std::string _pastelID, const SecureString& strKeyPass, std::string _address)
{
    CPastelIDRegTicket ticket(std::move(_pastelID));
    
    if (_address.empty()){
        CMasternode mn;
        if(!masterNodeCtrl.masternodeManager.Get(masterNodeCtrl.activeMasternode.outpoint, mn)) {
            throw std::runtime_error("This is not a active masternode. Only active MN can register its PastelID ");
        }
    
        //collateral address
        CTxDestination dest = mn.pubKeyCollateralAddress.GetID();
        ticket.address = std::move(EncodeDestination(dest));
    
        //outpoint hash
        ticket.outpoint = std::move(masterNodeCtrl.activeMasternode.outpoint.ToStringShort());
    } else {
        ticket.address = std::move(_address);
    }
    
    ticket.timestamp = std::time(nullptr);
	
	//signature of ticket hash
	CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
	ss << ticket.pastelID;
	ss << ticket.address;
	ss << ticket.outpoint;
	ss << ticket.timestamp;
	uint256 hash = ss.GetHash();
    ticket.signature = CPastelID::Sign(hash.begin(), hash.size(), ticket.pastelID, strKeyPass);
    
    return ticket;
}

std::string CPastelIDRegTicket::ToJSON()
{
	json jsonObj;
	jsonObj = {
		{"txid", ticketTnx},
		{"height", ticketBlock},
		{"ticket", {
			{"type", TicketName()},
			{"pastelID", pastelID},
			{"address", address},
			{"timeStamp", std::to_string(timestamp)},
			{"signature", ed_crypto::Hex_Encode(signature.data(), signature.size())},
			{"id_type", PastelIDType()}
		}}
	};

	if (!outpoint.empty())
		jsonObj["ticket"]["outpoint"] = outpoint;
	
	return jsonObj.dump(4);
}

/*static*/ bool CPastelIDRegTicket::FindTicketInDb(const std::string& key, CPastelIDRegTicket& ticket)
{
    //first try by PastelID
    ticket.pastelID = key;
    if (!masterNodeCtrl.masternodeTickets.FindTicket(ticket))
    {
        //if not, try by outpoint
        ticket.outpoint = key;
        if (!masterNodeCtrl.masternodeTickets.FindTicketBySecondaryKey(ticket)){
            //finally, clear outpoint and try by address
            ticket.outpoint.clear();
            ticket.address = key;
            if (!masterNodeCtrl.masternodeTickets.FindTicketBySecondaryKey(ticket))
                return false;
        }
    }
    return true;
}

// CArtRegTicket ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*static*/ CArtRegTicket CArtRegTicket::Create(std::string _ticket, const std::string& signatures, std::string _pastelID, const SecureString& strKeyPass,
                            std::string _keyOne, std::string _keyTwo, int _ticketBlock)
{
    CArtRegTicket ticket(std::move(_ticket));
    
    ticket.keyOne = std::move(_keyOne);
    ticket.keyTwo = std::move(_keyTwo);
    ticket.ticketBlock = _ticketBlock;
    
    ticket.mnPastelIDs[mainmnsign] = std::move(_pastelID);
    
    //signature of ticket hash
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << ticket.ticket;
    uint256 hash = ss.GetHash();
    ticket.mnSignatures[mainmnsign] = CPastelID::Sign(hash.begin(), hash.size(), ticket.mnPastelIDs[mainmnsign], strKeyPass);

    //other signatures
    auto jsonObj = json::parse(signatures);
    
    if (jsonObj.size() != 3){
        throw std::runtime_error("Signatures json is incorrect");
    }
    
    int i = 0;
    for (auto& el : jsonObj.items()) {
        
        if (el.key().empty()) {
            throw std::runtime_error("Signatures json is incorrect");
        }
        
        std::string pastelID = el.key();
        std::string signature = el.value();
        
        if (i == 0) {
            ticket.mnPastelIDs[i] = std::move(pastelID);
            ticket.mnSignatures[i] = ed_crypto::Base64_Decode(signature);
        } else if (i < 3){ //Main MN shall be at index 1
            ticket.mnPastelIDs[i+1] = std::move(pastelID);
            ticket.mnSignatures[i+1] = ed_crypto::Base64_Decode(signature);
        }
        i++;
    }
    
    return ticket;
}

bool CArtRegTicket::IsValid(bool preReg, std::string& errRet) const
{
    //1. Artist PastelID is registered (search for PastelID tickets) and is personal PastelID
    //2. Masternodes PastelIDs are registered (search for PastelID tickets) and are masternode PastelID's (not personal)
    //3. Masternodes beyond these PastelIDs, were in the top 10 at the block when the registration happened
    //4. Signatures matches included PastelIDs
    
    std::string err;
    
    for (int mn=0; mn<allsigns; mn++) {
        //1, 2
        CPastelIDRegTicket pastelIDticket;
        if (!CPastelIDRegTicket::FindTicketInDb(mnPastelIDs[mn], pastelIDticket)){
            if (mn == artistsign)
                errRet = strprintf("Artist PastelID is not registered");
            else
                errRet = strprintf("MN%d PastelID is not registered", mn);
            return false;
        }
        if (!pastelIDticket.IsValid(false, err)){
            if (mn == artistsign)
                errRet = strprintf("Artist PastelID is invalid - %s", err);
            else
                errRet = strprintf("MN%d PastelID is invalid - %s", mn, err);
            return false;
        }
        if (mn == artistsign) {
            if (!pastelIDticket.outpoint.empty()) {
                errRet = strprintf("Artist PastelID is NOT personal PastelID");
                return false;
            }
        } else {
            if (pastelIDticket.outpoint.empty()) {
                errRet = strprintf("MN%d PastelID is NOT mastennode PastelID", mn);
                return false;
            }
            
            //3
            /*Get-workers-for-block(ticketBlock)
            for (auto worker : workers){
                if pastelIDticket.outpoint not in workers
                    return false
            }
            */
        }
    }
    
    //4
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << ticket;
    uint256 hash = ss.GetHash();
    
    for (int mn=0; mn<allsigns; mn++) {
        if (!CPastelID::Verify(hash.begin(), hash.size(),
                mnSignatures[mn].data(), mnSignatures[mn].size(), mnPastelIDs[mn])){
            errRet = strprintf("MN%d signature is invalid", mn);
            return false;
        }
    }
    
    return true;
}

std::string CArtRegTicket::ToJSON()
{
	json jsonObj;
	jsonObj = {
			{"txid", ticketTnx},
			{"height", ticketBlock},
			{"ticket", {
				 {"type", TicketName()},
				 {"ticketBLOB", ticket},
				 {"key1", keyOne},
				 {"key2", keyTwo}
		 }}
	};
	
	return jsonObj.dump(4);
}

/*static*/ bool CArtRegTicket::FindTicketInDb(const std::string& key, CArtRegTicket& ticket)
{
    //First key
    ticket.keyOne = key;
    if (!masterNodeCtrl.masternodeTickets.FindTicket(ticket))
    {
        //if not, try by Second Key
        ticket.keyTwo = key;
        if (!masterNodeCtrl.masternodeTickets.FindTicketBySecondaryKey(ticket))
            return false;
    }
    return true;
}

// CArtActivateTicket ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CArtActivateTicket::CArtActivateTicket(std::string  txid, std::string _pastelID, const SecureString& strKeyPass)
{
	init(std::move(txid), std::move(_pastelID), strKeyPass);
}

void CArtActivateTicket::init(std::string&& txid, std::string&& _pastelID, const SecureString& strKeyPass)
{
	pastelID = std::move(_pastelID);
	regTicketTnxId = std::move(txid);

	//signature of ticket hash
	CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
	ss << pastelID;
	ss << regBlockHeight;
	ss << regTicketTnxId;
	uint256 hash = ss.GetHash();
	signature = CPastelID::Sign(hash.begin(), hash.size(), pastelID, strKeyPass);
}

std::string CArtActivateTicket::ToJSON()
{
	json jsonObj;
	jsonObj = {
			{"txid", ticketTnx},
			{"height", ticketBlock},
			{"ticket", {
				 {"type", TicketName()},
				 {"pastelID", pastelID},
				 {"reg_height", regBlockHeight},
				 {"reg_txid", regTicketTnxId},
				 {"signature", ed_crypto::Hex_Encode(signature.data(), signature.size())}
		 }}
	};
	
	return jsonObj.dump(4);
}

/*static*/ bool CArtActivateTicket::FindTicketInDb(const std::string& key, CArtActivateTicket& ticket)
{
    return false;
}

// CArtTradeTicket ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CArtTradeTicket::FindTicketInDb(const std::string& key, CArtTradeTicket& ticket)
{
    return false;
}

// CTakeDownTicket ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*static*/ bool CTakeDownTicket::FindTicketInDb(const std::string& key, CTakeDownTicket& ticket)
{
    return false;
}
