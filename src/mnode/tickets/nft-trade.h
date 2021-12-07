#pragma once
// Copyright (c) 2018-2021 The Pastel Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.
#include <mnode/tickets/ticket.h>
#include <map_types.h>

#include <tuple>
#include <optional>

// forward ticket class declaration
class CNFTTradeTicket;

// ticket vector
using NFTTradeTickets_t = std::vector<CNFTTradeTicket>;

// NFT Trade Ticket /////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	"ticket": {
		"type": "trade",
		"pastelID": "",     //PastelID of the buyer
		"sell_txid": "",    //txid with sale ticket
		"buy_txid": "",     //txid with buy ticket
		"nft_txid": "",     //txid with either 1) NFT activation ticket or 2) trade ticket in it
		"price": "",
		"reserved": "",
		"signature": ""
	},
 */
using txid_serial_tuple_t = std::tuple<std::string, std::string>;

class CNFTTradeTicket : public CPastelTicket
{
public:
    std::string pastelID;
    std::string sellTxnId;
    std::string buyTxnId;
    std::string NFTTxnId;
    std::string nftRegTxnId;
    std::string nftCopySerialNr;

    unsigned int price{};
    std::string reserved;
    v_uint8 signature;

public:
    CNFTTradeTicket() = default;

    explicit CNFTTradeTicket(std::string _pastelID) : pastelID(std::move(_pastelID))
    {
    }

    TicketID ID() const noexcept override { return TicketID::Trade; }
    static TicketID GetID() { return TicketID::Trade; }

    void Clear() noexcept override
    {
        pastelID.clear();
        sellTxnId.clear();
        buyTxnId.clear();
        NFTTxnId.clear();
        nftRegTxnId.clear();
        nftCopySerialNr.clear();
        price = 0;
        reserved.clear();
        signature.clear();
    }
    std::string KeyOne() const noexcept override { return sellTxnId; }
    std::string KeyTwo() const noexcept override { return buyTxnId; }
    std::string MVKeyOne() const noexcept override { return pastelID; }
    std::string MVKeyTwo() const noexcept override { return NFTTxnId; }
    std::string MVKeyThree() const noexcept override { return nftRegTxnId; }

    bool HasKeyTwo() const noexcept override { return true; }
    bool HasMVKeyOne() const noexcept override { return true; }
    bool HasMVKeyTwo() const noexcept override { return true; }
    bool HasMVKeyThree() const noexcept override { return true; }

    void SetKeyOne(std::string val) override { sellTxnId = std::move(val); }

    std::string ToJSON() const noexcept override;
    std::string ToStr() const noexcept override;
    bool IsValid(const bool bPreReg, const int nDepth) const override;

    void SerializationOp(CDataStream& s, const SERIALIZE_ACTION ser_action) override
    {
        const bool bRead = ser_action == SERIALIZE_ACTION::Read;
        std::string error;
        if (!VersionMgmt(error, bRead))
            throw std::runtime_error(error);
        READWRITE(pastelID);
        READWRITE(m_nVersion);
        // v0
        READWRITE(sellTxnId);
        READWRITE(buyTxnId);
        READWRITE(NFTTxnId);
        READWRITE(price);
        READWRITE(reserved);
        READWRITE(signature);
        READWRITE(m_nTimestamp);
        READWRITE(m_txid);
        READWRITE(m_nBlock);
        READWRITE(nftRegTxnId);
        READWRITE(nftCopySerialNr);
    }

    CAmount GetExtraOutputs(std::vector<CTxOut>& outputs) const override;

    static CNFTTradeTicket Create(std::string _sellTxnId, std::string _buyTxnId, std::string _pastelID, SecureString&& strKeyPass);
    static bool FindTicketInDb(const std::string& key, CNFTTradeTicket& ticket);

    static NFTTradeTickets_t FindAllTicketByPastelID(const std::string& pastelID);
    static NFTTradeTickets_t FindAllTicketByNFTTxnID(const std::string& NFTTxnId);
    static NFTTradeTickets_t FindAllTicketByRegTnxID(const std::string& nftRegTxnId);

    static bool CheckTradeTicketExistBySellTicket(const std::string& _sellTxnId);
    static bool CheckTradeTicketExistByBuyTicket(const std::string& _buyTxnId);
    static bool GetTradeTicketBySellTicket(const std::string& _sellTxnId, CNFTTradeTicket& ticket);
    static bool GetTradeTicketByBuyTicket(const std::string& _buyTxnId, CNFTTradeTicket& ticket);
    static mu_strings GetPastelIdAndTxIdWithTopHeightPerCopy(const NFTTradeTickets_t& allTickets);

    std::unique_ptr<CPastelTicket> FindNFTRegTicket() const;

    void SetNFTRegTicketTxid(const std::string& sNftRegTxid);
    const std::string GetNFTRegTicketTxid() const;
    void SetCopySerialNr(const std::string& nftCopySerialNr);
    const std::string& GetCopySerialNr() const;

    static std::optional<txid_serial_tuple_t> GetNFTRegTxIDAndSerialIfResoldNft(const std::string& _txid);
};