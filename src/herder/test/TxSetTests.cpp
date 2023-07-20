// Copyright 2022 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "herder/TxSetFrame.h"
#include "herder/test/TestTxSetUtils.h"
#include "ledger/LedgerManager.h"
#include "lib/catch.hpp"
#include "main/Application.h"
#include "main/Config.h"
#include "test/TestAccount.h"
#include "test/TestUtils.h"
#include "test/TxTests.h"
#include "test/test.h"
#include "util/ProtocolVersion.h"

namespace stellar
{
namespace
{
using namespace txtest;

#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
TEST_CASE("generalized tx set XDR validation", "[txset]")
{
    Config cfg(getTestConfig());
    cfg.LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_UPGRADE_LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    VirtualClock clock;
    Application::pointer app = createTestApplication(clock, cfg);

    GeneralizedTransactionSet xdrTxSet(1);
    xdrTxSet.v1TxSet().previousLedgerHash =
        app->getLedgerManager().getLastClosedLedgerHeader().hash;

    SECTION("no phases")
    {
        auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
        REQUIRE(!txSet->checkValidStructure());
    }
    SECTION("too many phases")
    {
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();
        auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
        REQUIRE(!txSet->checkValidStructure());
    }
    SECTION("incorrect base fee order")
    {
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();
        for (int i = 0; i < xdrTxSet.v1TxSet().phases.size(); ++i)
        {
            SECTION("phase " + std::to_string(i))
            {
                SECTION("all components discounted")
                {

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1400;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1600;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(!txSet->checkValidStructure());
                }
                SECTION("non-discounted component out of place")
                {

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1600;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(!txSet->checkValidStructure());
                }
                SECTION(
                    "with non-discounted component, discounted out of place")
                {
                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1400;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(!txSet->checkValidStructure());
                }
            }
        }
    }
    SECTION("duplicate base fee")
    {
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();
        for (int i = 0; i < xdrTxSet.v1TxSet().phases.size(); ++i)
        {
            SECTION("phase " + std::to_string(i))
            {
                SECTION("duplicate discounts")
                {
                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1600;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(!txSet->checkValidStructure());
                }
                SECTION("duplicate non-discounted components")
                {

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(!txSet->checkValidStructure());
                }
            }
        }
    }
    SECTION("empty component")
    {
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();

        for (int i = 0; i < xdrTxSet.v1TxSet().phases.size(); ++i)
        {
            SECTION("phase " + std::to_string(i))
            {
                xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                    TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);

                auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                REQUIRE(!txSet->checkValidStructure());
            }
        }
    }
    SECTION("wrong tx type in phases")
    {
        xdrTxSet.v1TxSet().phases.emplace_back();
        xdrTxSet.v1TxSet().phases.emplace_back();
        SECTION("classic phase")
        {
            xdrTxSet.v1TxSet().phases[1].v0Components().emplace_back(
                TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
            xdrTxSet.v1TxSet()
                .phases[1]
                .v0Components()
                .back()
                .txsMaybeDiscountedFee()
                .txs.emplace_back();
        }
        SECTION("soroban phase")
        {
            xdrTxSet.v1TxSet().phases[0].v0Components().emplace_back(
                TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
            xdrTxSet.v1TxSet()
                .phases[0]
                .v0Components()
                .back()
                .txsMaybeDiscountedFee()
                .txs.emplace_back();

            auto& txEnv = xdrTxSet.v1TxSet()
                              .phases[0]
                              .v0Components()
                              .back()
                              .txsMaybeDiscountedFee()
                              .txs.back();

            txEnv.v0().tx.operations.emplace_back();
            txEnv.v0().tx.operations.back().body.type(INVOKE_HOST_FUNCTION);
        }
        auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
        REQUIRE(!txSet->checkValidStructure());
    }
    SECTION("valid XDR")
    {

        for (int i = 0; i < xdrTxSet.v1TxSet().phases.size(); ++i)
        {
            auto maybeAddSorobanOp = [&](GeneralizedTransactionSet& txSet) {
                if (i == 1)
                {
                    auto& txEnv = xdrTxSet.v1TxSet()
                                      .phases[i]
                                      .v0Components()
                                      .back()
                                      .txsMaybeDiscountedFee()
                                      .txs.back();
                    txEnv.v0().tx.operations.emplace_back();
                    txEnv.v0().tx.operations.back().body.type(
                        INVOKE_HOST_FUNCTION); // tx->isSoroban() ==
                                               // true
                }
            };
            SECTION("phase " + std::to_string(i))
            {
                SECTION("no transactions")
                {
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(txSet->checkValidStructure());
                }
                SECTION("single component")
                {
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();
                    maybeAddSorobanOp(xdrTxSet);
                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(txSet->checkValidStructure());
                }
                SECTION("multiple components")
                {
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    xdrTxSet.v1TxSet().phases.emplace_back();
                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();
                    maybeAddSorobanOp(xdrTxSet);

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1400;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();
                    maybeAddSorobanOp(xdrTxSet);

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1500;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();
                    maybeAddSorobanOp(xdrTxSet);

                    xdrTxSet.v1TxSet().phases[i].v0Components().emplace_back(
                        TXSET_COMP_TXS_MAYBE_DISCOUNTED_FEE);
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .baseFee.activate() = 1600;
                    xdrTxSet.v1TxSet()
                        .phases[i]
                        .v0Components()
                        .back()
                        .txsMaybeDiscountedFee()
                        .txs.emplace_back();
                    maybeAddSorobanOp(xdrTxSet);

                    auto txSet = TxSetFrame::makeFromWire(*app, xdrTxSet);
                    REQUIRE(txSet->checkValidStructure());
                }
            }
        }
    }
}

TEST_CASE("generalized tx set XDR conversion", "[txset]")
{
    VirtualClock clock;
    auto cfg = getTestConfig();
    cfg.LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_UPGRADE_LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_LEDGER_MAX_SOROBAN_TX_COUNT = 5;
    Application::pointer app = createTestApplication(clock, cfg);
    auto root = TestAccount::createRoot(*app);
    int accountId = 0;
    auto createTxs = [&](int cnt, int fee, bool isSoroban = false) {
        std::vector<TransactionFrameBasePtr> txs;
        for (int i = 0; i < cnt; ++i)
        {
            auto source =
                root.create("unique " + std::to_string(accountId++),
                            app->getLedgerManager().getLastMinBalance(2));
            if (isSoroban)
            {
                SorobanResources resources;
                resources.instructions = 800'000;
                resources.readBytes = 1000;
                resources.writeBytes = 1000;
                resources.extendedMetaDataSizeBytes = 3000;
                txs.emplace_back(createUploadWasmTx(*app, source, fee,
                                                    /* refundableFee */ 1200,
                                                    resources));
            }
            else
            {
                txs.emplace_back(transactionFromOperations(
                    *app, source.getSecretKey(), source.nextSequenceNumber(),
                    {createAccount(getAccount(std::to_string(i)).getPublicKey(),
                                   1)},
                    fee));
            }
        }
        return txs;
    };

    auto checkXdrRoundtrip = [&](GeneralizedTransactionSet const& txSetXdr) {
        auto frame = TxSetFrame::makeFromWire(*app, txSetXdr);
        REQUIRE(frame->checkValid(*app, 0, 0));
        GeneralizedTransactionSet newXdr;
        frame->toXDR(newXdr);
        REQUIRE(newXdr == txSetXdr);
    };

    SECTION("empty set")
    {
        auto txSetFrame = testtxset::makeNonValidatedGeneralizedTxSet(
            {{}, {}}, *app,
            app->getLedgerManager().getLastClosedLedgerHeader().hash);

        GeneralizedTransactionSet txSetXdr;
        txSetFrame->toXDR(txSetXdr);
        REQUIRE(txSetXdr.v1TxSet().phases[0].v0Components().empty());
        checkXdrRoundtrip(txSetXdr);
    }
    SECTION("one discounted component set")
    {
        auto txSetFrame = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(1234LL, createTxs(5, 1234))}, {}}, *app,
            app->getLedgerManager().getLastClosedLedgerHeader().hash);

        GeneralizedTransactionSet txSetXdr;
        txSetFrame->toXDR(txSetXdr);
        REQUIRE(txSetXdr.v1TxSet().phases[0].v0Components().size() == 1);
        REQUIRE(*txSetXdr.v1TxSet()
                     .phases[0]
                     .v0Components()[0]
                     .txsMaybeDiscountedFee()
                     .baseFee == 1234);
        REQUIRE(txSetXdr.v1TxSet()
                    .phases[0]
                    .v0Components()[0]
                    .txsMaybeDiscountedFee()
                    .txs.size() == 5);
        checkXdrRoundtrip(txSetXdr);
    }
    SECTION("one non-discounted component set")
    {
        auto txSetFrame = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(std::nullopt, createTxs(5, 4321))}, {}}, *app,
            app->getLedgerManager().getLastClosedLedgerHeader().hash);

        GeneralizedTransactionSet txSetXdr;
        txSetFrame->toXDR(txSetXdr);
        REQUIRE(txSetXdr.v1TxSet().phases[0].v0Components().size() == 1);
        REQUIRE(!txSetXdr.v1TxSet()
                     .phases[0]
                     .v0Components()[0]
                     .txsMaybeDiscountedFee()
                     .baseFee);
        REQUIRE(txSetXdr.v1TxSet()
                    .phases[0]
                    .v0Components()[0]
                    .txsMaybeDiscountedFee()
                    .txs.size() == 5);
        checkXdrRoundtrip(txSetXdr);
    }
    SECTION("multiple component sets")
    {
        auto txSetFrame = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(12345LL, createTxs(3, 12345)),
              std::make_pair(123LL, createTxs(1, 123)),
              std::make_pair(1234LL, createTxs(2, 1234)),
              std::make_pair(std::nullopt, createTxs(4, 4321))},
             {}},
            *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

        GeneralizedTransactionSet txSetXdr;
        txSetFrame->toXDR(txSetXdr);
        auto const& comps = txSetXdr.v1TxSet().phases[0].v0Components();
        REQUIRE(comps.size() == 4);
        REQUIRE(!comps[0].txsMaybeDiscountedFee().baseFee);
        REQUIRE(comps[0].txsMaybeDiscountedFee().txs.size() == 4);
        REQUIRE(*comps[1].txsMaybeDiscountedFee().baseFee == 123);
        REQUIRE(comps[1].txsMaybeDiscountedFee().txs.size() == 1);
        REQUIRE(*comps[2].txsMaybeDiscountedFee().baseFee == 1234);
        REQUIRE(comps[2].txsMaybeDiscountedFee().txs.size() == 2);
        REQUIRE(*comps[3].txsMaybeDiscountedFee().baseFee == 12345);
        REQUIRE(comps[3].txsMaybeDiscountedFee().txs.size() == 3);
        checkXdrRoundtrip(txSetXdr);
    }
    SECTION("built from transactions")
    {
        auto const& lclHeader =
            app->getLedgerManager().getLastClosedLedgerHeader();
        std::vector<TransactionFrameBasePtr> txs =
            createTxs(5, lclHeader.header.baseFee, /* isSoroban */ false);
        std::vector<TransactionFrameBasePtr> sorobanTxs =
            createTxs(5, 10'000'000, /* isSoroban */ true);

        SECTION("classic only")
        {
            auto txSet = TxSetFrame::makeFromTransactions(txs, *app, 0, 0);
            GeneralizedTransactionSet txSetXdr;
            txSet->toXDR(txSetXdr);
            REQUIRE(txSetXdr.v1TxSet().phases.size() == 2);
            REQUIRE(txSetXdr.v1TxSet().phases[0].v0Components().size() == 1);
            REQUIRE(*txSetXdr.v1TxSet()
                         .phases[0]
                         .v0Components()[0]
                         .txsMaybeDiscountedFee()
                         .baseFee == lclHeader.header.baseFee);
            REQUIRE(txSetXdr.v1TxSet()
                        .phases[0]
                        .v0Components()[0]
                        .txsMaybeDiscountedFee()
                        .txs.size() == 5);
            checkXdrRoundtrip(txSetXdr);
        }
        SECTION("classic and soroban")
        {
            SECTION("valid")
            {
                SECTION("minimum base fee")
                {
                    auto txSet = TxSetFrame::makeFromTransactions(
                        {txs, sorobanTxs}, *app, 0, 0);
                    GeneralizedTransactionSet txSetXdr;
                    txSet->toXDR(txSetXdr);
                    REQUIRE(txSetXdr.v1TxSet().phases.size() == 2);
                    for (auto const& phase : txSetXdr.v1TxSet().phases)
                    {
                        // Base inclusion fee is 100 for all phases since no
                        // surge pricing kicked in
                        REQUIRE(phase.v0Components().size() == 1);
                        REQUIRE(*phase.v0Components()[0]
                                     .txsMaybeDiscountedFee()
                                     .baseFee == lclHeader.header.baseFee);
                        REQUIRE(phase.v0Components()[0]
                                    .txsMaybeDiscountedFee()
                                    .txs.size() == 5);
                    }
                    checkXdrRoundtrip(txSetXdr);
                }
                SECTION("higher base fee")
                {
                    // generate more soroban txs with higher fee to trigger
                    // surge pricing
                    auto higherFeeSorobanTxs =
                        createTxs(5, 20'000'000, /* isSoroban */ true);
                    sorobanTxs.insert(sorobanTxs.begin(),
                                      higherFeeSorobanTxs.begin(),
                                      higherFeeSorobanTxs.end());
                    auto txSet = TxSetFrame::makeFromTransactions(
                        {txs, sorobanTxs}, *app, 0, 100);
                    GeneralizedTransactionSet txSetXdr;
                    txSet->toXDR(txSetXdr);
                    REQUIRE(txSetXdr.v1TxSet().phases.size() == 2);
                    for (int i = 0; i < txSetXdr.v1TxSet().phases.size(); i++)
                    {
                        auto const& phase = txSetXdr.v1TxSet().phases[i];
                        auto expectedBaseFee =
                            i == 0 ? lclHeader.header.baseFee
                                   : higherFeeSorobanTxs[0]->getInclusionFee();
                        REQUIRE(phase.v0Components().size() == 1);
                        REQUIRE(*phase.v0Components()[0]
                                     .txsMaybeDiscountedFee()
                                     .baseFee == expectedBaseFee);
                        REQUIRE(phase.v0Components()[0]
                                    .txsMaybeDiscountedFee()
                                    .txs.size() == 5);
                    }
                    checkXdrRoundtrip(txSetXdr);
                }
            }
            SECTION("invalid, soroban tx in wrong phase")
            {
                sorobanTxs[4] = txs[0];
                REQUIRE_THROWS_WITH(TxSetFrame::makeFromTransactions(
                                        {txs, sorobanTxs}, *app, 0, 0),
                                    "TxSetFrame::makeFromTransactions: phases "
                                    "contain txs of wrong type");
            }
            SECTION("invalid, classic tx in wrong phase")
            {
                txs[4] = sorobanTxs[0];
                REQUIRE_THROWS_WITH(TxSetFrame::makeFromTransactions(
                                        {txs, sorobanTxs}, *app, 0, 0),
                                    "TxSetFrame::makeFromTransactions: phases "
                                    "contain txs of wrong type");
            }
        }
    }
}

TEST_CASE("generalized tx set with multiple txs per source account", "[txset]")
{
    VirtualClock clock;
    auto cfg = getTestConfig();
    cfg.LIMIT_TX_QUEUE_SOURCE_ACCOUNT = true;
    cfg.LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_UPGRADE_LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    Application::pointer app = createTestApplication(clock, cfg);
    auto root = TestAccount::createRoot(*app);
    int accountId = 1;

    auto createTx = [&](int opCnt, int fee, bool unique) {
        std::vector<Operation> ops;
        for (int i = 0; i < opCnt; ++i)
        {
            ops.emplace_back(createAccount(
                getAccount(std::to_string(accountId++)).getPublicKey(), 1));
        }

        if (unique)
        {
            // Create a new unique accounts to ensure there are no collisions
            auto source =
                root.create("unique " + std::to_string(accountId),
                            app->getLedgerManager().getLastMinBalance(2));
            return transactionFromOperations(*app, source.getSecretKey(),
                                             source.nextSequenceNumber(), ops,
                                             fee);
        }
        else
        {
            return transactionFromOperations(
                *app, root.getSecretKey(), root.nextSequenceNumber(), ops, fee);
        }
    };

    SECTION("invalid")
    {
        auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(
                 500,
                 std::vector<TransactionFrameBasePtr>{
                     createTx(1, 1000, false), createTx(3, 1500, false)})},
             {}},
            *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

        REQUIRE(!txSet->checkValid(*app, 0, 0));
    }
    SECTION("valid")
    {
        auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(
                 500,
                 std::vector<TransactionFrameBasePtr>{
                     createTx(1, 1000, true), createTx(3, 1500, true)})},
             {}},
            *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

        REQUIRE(txSet->checkValid(*app, 0, 0));
    }
    SECTION("invalid, classic and soroban")
    {
        SorobanResources resources;
        resources.instructions = 800'000;
        resources.readBytes = 1000;
        resources.writeBytes = 1000;
        resources.extendedMetaDataSizeBytes = 3000;
        uint32_t inclusionFee = 500;
        uint32_t refundableFee = 10'000;
        auto sorobanTx = createUploadWasmTx(*app, root, inclusionFee,
                                            refundableFee, resources);
        setValidTotalFee(sorobanTx, inclusionFee, refundableFee, *app, root);
        // Make sure fees got computed correctly
        REQUIRE(sorobanTx->getInclusionFee() == inclusionFee);

        auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(
                 500,
                 std::vector<TransactionFrameBasePtr>{
                     createTx(1, 1000, false), createTx(3, 1500, false)})},
             {std::make_pair(500,
                             std::vector<TransactionFrameBasePtr>{sorobanTx})}},
            *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

        REQUIRE(!txSet->checkValid(*app, 0, 0));
    }
}

TEST_CASE("generalized tx set fees", "[txset]")
{
    VirtualClock clock;
    auto cfg = getTestConfig();
    cfg.LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_UPGRADE_LEDGER_PROTOCOL_VERSION =
        static_cast<uint32_t>(GENERALIZED_TX_SET_PROTOCOL_VERSION);
    cfg.TESTING_LEDGER_MAX_SOROBAN_TX_COUNT = 10;

    Application::pointer app = createTestApplication(clock, cfg);
    auto root = TestAccount::createRoot(*app);
    int accountId = 1;
    uint32_t refundableFee = 10'000;

    auto createTx = [&](int opCnt, int inclusionFee, bool isSoroban = false) {
        auto source = root.create("unique " + std::to_string(accountId++),
                                  app->getLedgerManager().getLastMinBalance(2));
        if (isSoroban)
        {
            SorobanResources resources;
            resources.instructions = 800'000;
            resources.readBytes = 1000;
            resources.writeBytes = 1000;
            resources.extendedMetaDataSizeBytes = 3000;
            auto tx = createUploadWasmTx(*app, source, inclusionFee,
                                         refundableFee, resources);
            setValidTotalFee(tx, inclusionFee, refundableFee, *app, source);
            REQUIRE(tx->getInclusionFee() == inclusionFee);
            return tx;
        }
        else
        {
            std::vector<Operation> ops;
            for (int i = 0; i < opCnt; ++i)
            {
                ops.emplace_back(createAccount(
                    getAccount(std::to_string(accountId++)).getPublicKey(), 1));
            }
            return transactionFromOperations(*app, source.getSecretKey(),
                                             source.nextSequenceNumber(), ops,
                                             inclusionFee);
        }
    };

    SECTION("valid txset")
    {
        auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
            {{std::make_pair(
                  500, std::vector<TransactionFrameBasePtr>{createTx(1, 1000),
                                                            createTx(3, 1500)}),
              std::make_pair(
                  1000,
                  std::vector<TransactionFrameBasePtr>{
                      createTx(4, 5000), createTx(1, 1000), createTx(5, 6000)}),
              std::make_pair(std::nullopt,
                             std::vector<TransactionFrameBasePtr>{
                                 createTx(2, 10000), createTx(5, 100000)})},
             {std::make_pair(500,
                             std::vector<TransactionFrameBasePtr>{
                                 createTx(1, 1000, /* isSoroban */ true),
                                 createTx(1, 500, /* isSoroban */ true)}),
              std::make_pair(1000,
                             std::vector<TransactionFrameBasePtr>{
                                 createTx(1, 1250, /* isSoroban */ true),
                                 createTx(1, 1000, /* isSoroban */ true),
                                 createTx(1, 1200, /* isSoroban */ true)}),
              std::make_pair(std::nullopt,
                             std::vector<TransactionFrameBasePtr>{
                                 createTx(1, 5000, /* isSoroban */ true),
                                 createTx(1, 20000, /* isSoroban */ true)})}},
            *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

        REQUIRE(txSet->checkValid(*app, 0, 0));
        for (int i = 0; i < TxSetFrame::Phase::PHASE_COUNT; ++i)
        {
            std::vector<std::optional<int64_t>> fees;
            for (auto const& tx :
                 txSet->getTxsForPhase(static_cast<TxSetFrame::Phase>(i)))
            {
                fees.push_back(
                    txSet->getTxBaseFee(tx, app->getLedgerManager()
                                                .getLastClosedLedgerHeader()
                                                .header));
            }
            std::sort(fees.begin(), fees.end());
            REQUIRE(fees == std::vector<std::optional<int64_t>>{
                                std::nullopt, std::nullopt, 500, 500, 1000,
                                1000, 1000});
        }
    }
    SECTION("tx with too low discounted fee")
    {
        SECTION("classic")
        {
            auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
                {{std::make_pair(
                     500,
                     std::vector<TransactionFrameBasePtr>{createTx(2, 999)})},
                 {}},
                *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

            REQUIRE(!txSet->checkValid(*app, 0, 0));
        }
        SECTION("soroban")
        {
            auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
                {{},
                 {std::make_pair(500,
                                 std::vector<TransactionFrameBasePtr>{
                                     createTx(1, 499, /* isSoroban */ true)})}},
                *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

            REQUIRE(!txSet->checkValid(*app, 0, 0));
        }
    }

    SECTION("tx with too low non-discounted fee")
    {
        SECTION("classic")
        {
            auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
                {{std::make_pair(
                     std::nullopt,
                     std::vector<TransactionFrameBasePtr>{createTx(2, 199)})},
                 {}},
                *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

            REQUIRE(!txSet->checkValid(*app, 0, 0));
        }
        SECTION("soroban")
        {
            auto txSet = testtxset::makeNonValidatedGeneralizedTxSet(
                {{},
                 {std::make_pair(
                     std::nullopt,
                     std::vector<TransactionFrameBasePtr>{createTx(1, 199)})}},
                *app, app->getLedgerManager().getLastClosedLedgerHeader().hash);

            REQUIRE(!txSet->checkValid(*app, 0, 0));
        }
    }
}
#endif

} // namespace
} // namespace stellar
