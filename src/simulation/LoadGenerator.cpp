// Copyright 2015 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "simulation/LoadGenerator.h"
#include "herder/Herder.h"
#include "ledger/LedgerManager.h"
#include "ledger/LedgerTxn.h"
#include "ledger/LedgerTxnEntry.h"
#include "main/Config.h"
#include "overlay/OverlayManager.h"
#include "test/TestAccount.h"
#include "test/TxTests.h"
#include "transactions/TransactionBridge.h"
#include "transactions/TransactionUtils.h"
#include "transactions/test/SorobanTxTestUtils.h"
#include "util/Logging.h"
#include "util/Math.h"
#include "util/Timer.h"
#include "util/XDRCereal.h"
#include "util/numeric.h"
#include "util/types.h"

#include "database/Database.h"

#include "xdrpp/marshal.h"

#include "medida/meter.h"
#include "medida/metrics_registry.h"

#include "ledger/test/LedgerTestUtils.h"
#include <Tracy.hpp>
#include <cmath>
#include <crypto/SHA.h>
#include <fmt/format.h>
#include <iomanip>
#include <set>

namespace stellar
{

using namespace std;
using namespace txtest;

// Units of load are scheduled at 100ms intervals.
const uint32_t LoadGenerator::STEP_MSECS = 100;

// If submission fails with txBAD_SEQ, attempt refreshing the account or
// re-submitting a new payment
const uint32_t LoadGenerator::TX_SUBMIT_MAX_TRIES = 10;

// After successfully submitting desired load, wait a bit to let it get into the
// ledger.
const uint32_t LoadGenerator::TIMEOUT_NUM_LEDGERS = 20;

// After successfully submitting desired load, wait for this many ledgers
// without checking for account consistency.
const uint32_t LoadGenerator::COMPLETION_TIMEOUT_WITHOUT_CHECKS = 4;

// Minimum unique account multiplier. This is used to calculate the minimum
// number of accounts needed to sustain desired tx/s rate (this provides a
// buffer in case loadgen is unstable and needs more accounts)
const uint32_t LoadGenerator::MIN_UNIQUE_ACCOUNT_MULTIPLIER = 3;

LoadGenerator::LoadGenerator(Application& app)
    : mMinBalance(0)
    , mLastSecond(0)
    , mApp(app)
    , mTotalSubmitted(0)
    , mLoadgenComplete(
          mApp.getMetrics().NewMeter({"loadgen", "run", "complete"}, "run"))
    , mLoadgenFail(
          mApp.getMetrics().NewMeter({"loadgen", "run", "failed"}, "run"))
{
    createRootAccount();
}

LoadGenMode
LoadGenerator::getMode(std::string const& mode)
{
    if (mode == "create")
    {
        return LoadGenMode::CREATE;
    }
    else if (mode == "pay")
    {
        return LoadGenMode::PAY;
    }
    else if (mode == "pretend")
    {
        return LoadGenMode::PRETEND;
    }
    else if (mode == "mixed_txs")
    {
        return LoadGenMode::MIXED_TXS;
    }
    else if (mode == "soroban_upload")
    {
        return LoadGenMode::SOROBAN_UPLOAD;
    }
    else if (mode == "soroban_invoke_setup")
    {
        return LoadGenMode::SOROBAN_INVOKE_SETUP;
    }
    else if (mode == "soroban_invoke")
    {
        return LoadGenMode::SOROBAN_INVOKE;
    }
    else
    {
        throw std::runtime_error(
            fmt::format(FMT_STRING("Unknown loadgen mode: {}"), mode));
    }
}

int
generateFee(std::optional<uint32_t> maxGeneratedFeeRate, Application& app,
            size_t opsCnt)
{
    int fee = 0;
    auto baseFee = app.getLedgerManager().getLastTxFee();

    if (maxGeneratedFeeRate)
    {
        auto feeRateDistr =
            uniform_int_distribution<uint32_t>(baseFee, *maxGeneratedFeeRate);
        // Add a bit more fee to get non-integer fee rates, such that
        // `floor(fee / opsCnt) == feeRate`, but
        // `fee / opsCnt >= feeRate`.
        // This is to create a bit more realistic fee structure: in reality not
        // every transaction would necessarily have the `fee == ops_count *
        // some_int`. This also would exercise more code paths/logic during the
        // transaction comparisons.
        auto fractionalFeeDistr = uniform_int_distribution<uint32_t>(
            0, static_cast<uint32_t>(opsCnt) - 1);
        fee = static_cast<uint32_t>(opsCnt) * feeRateDistr(gRandomEngine) +
              fractionalFeeDistr(gRandomEngine);
    }
    else
    {
        fee = static_cast<int>(opsCnt * baseFee);
    }

    return fee;
}

void
LoadGenerator::createRootAccount()
{
    if (!mRoot)
    {
        auto rootTestAccount = TestAccount::createRoot(mApp);
        mRoot = make_shared<TestAccount>(rootTestAccount);
        if (!loadAccount(mRoot, mApp))
        {
            CLOG_ERROR(LoadGen, "Could not retrieve root account!");
        }
    }
}

unsigned short
LoadGenerator::chooseOpCount(Config const& cfg) const
{
    if (cfg.LOADGEN_OP_COUNT_FOR_TESTING.empty())
    {
        return 1;
    }
    else
    {
        std::discrete_distribution<uint32> distribution(
            cfg.LOADGEN_OP_COUNT_DISTRIBUTION_FOR_TESTING.begin(),
            cfg.LOADGEN_OP_COUNT_DISTRIBUTION_FOR_TESTING.end());
        return cfg.LOADGEN_OP_COUNT_FOR_TESTING[distribution(gRandomEngine)];
    }
}

int64_t
LoadGenerator::getTxPerStep(uint32_t txRate, std::chrono::seconds spikeInterval,
                            uint32_t spikeSize)
{
    if (!mStartTime)
    {
        throw std::runtime_error("Load generation start time must be set");
    }

    auto& stepMeter =
        mApp.getMetrics().NewMeter({"loadgen", "step", "count"}, "step");
    stepMeter.Mark();

    auto now = mApp.getClock().now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - *mStartTime);
    auto txs =
        bigDivideOrThrow(elapsed.count(), txRate, 1000, Rounding::ROUND_DOWN);
    if (spikeInterval.count() > 0)
    {
        txs += bigDivideOrThrow(
                   std::chrono::duration_cast<std::chrono::seconds>(elapsed)
                       .count(),
                   1, spikeInterval.count(), Rounding::ROUND_DOWN) *
               spikeSize;
    }

    if (txs <= mTotalSubmitted)
    {
        return 0;
    }

    return txs - mTotalSubmitted;
}

void
LoadGenerator::cleanupAccounts()
{
    ZoneScoped;

    // Check if creation source accounts have been created
    for (auto it = mCreationSourceAccounts.begin();
         it != mCreationSourceAccounts.end();)
    {
        if (loadAccount(it->second, mApp))
        {
            mAccountsAvailable.insert(it->first);
            it = mCreationSourceAccounts.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // "Free" any accounts that aren't used by the tx queue anymore
    for (auto it = mAccountsInUse.begin(); it != mAccountsInUse.end();)
    {
        auto accIt = mAccounts.find(*it);
        releaseAssert(accIt != mAccounts.end());
        if (!mApp.getHerder().sourceAccountPending(
                accIt->second->getPublicKey()))
        {
            mAccountsAvailable.insert(*it);
            it = mAccountsInUse.erase(it);
        }
        else
        {
            it++;
        }
    }
}

void
LoadGenerator::reset()
{
    mAccounts.clear();
    mAccountsInUse.clear();
    mAccountsAvailable.clear();
    mCreationSourceAccounts.clear();
    mPendingCodeKey.reset();
    mContractInstances.clear();
    mLoadTimer.reset();
    mRoot.reset();
    mStartTime.reset();
    mTotalSubmitted = 0;
    mWaitTillCompleteForLedgers = 0;
    mFailed = false;
    mStarted = false;
    mInitialAccountsCreated = false;
}

// Schedule a callback to generateLoad() STEP_MSECS milliseconds from now.
void
LoadGenerator::scheduleLoadGeneration(GeneratedLoadConfig cfg)
{
    std::optional<std::string> errorMsg;
    // If previously scheduled step of load did not succeed, fail this loadgen
    // run.
    if (mFailed)
    {
        errorMsg = "Load generation failed, ensure correct "
                   "number parameters are set and accounts are "
                   "created, or retry with smaller tx rate.";
    }
    // During load submission, we must have enough unique source accounts (with
    // a buffer) to accommodate the desired tx rate.
    if (cfg.mode != LoadGenMode::CREATE && cfg.nTxs > cfg.nAccounts &&
        (cfg.txRate * Herder::EXP_LEDGER_TIMESPAN_SECONDS.count()) *
                MIN_UNIQUE_ACCOUNT_MULTIPLIER >
            cfg.nAccounts)
    {
        errorMsg = fmt::format(
            "Tx rate is too high, there are not enough unique accounts. Make "
            "sure there are at least {}x "
            "unique accounts than desired number of transactions per ledger.",
            MIN_UNIQUE_ACCOUNT_MULTIPLIER);
    }

    if ((cfg.mode == LoadGenMode::SOROBAN_INVOKE ||
         cfg.mode == LoadGenMode::SOROBAN_UPLOAD ||
         cfg.mode == LoadGenMode::SOROBAN_INVOKE_SETUP) &&
        protocolVersionIsBefore(mApp.getLedgerManager()
                                    .getLastClosedLedgerHeader()
                                    .header.ledgerVersion,
                                SOROBAN_PROTOCOL_VERSION))
    {
        errorMsg = "Soroban modes require protocol version 20 or higher";
    }

    if (cfg.mode == LoadGenMode::SOROBAN_INVOKE)
    {
        auto& sorobanCfg = mApp.getLedgerManager().getSorobanNetworkConfig();
        if (mContractInstanceKeys.size() < cfg.nAccounts)
        {
            errorMsg = "must run SOROBAN_INVOKE_SETUP with at least nAccounts";
        }
        else if (cfg.nDataEntriesHigh > sorobanCfg.mTxMaxWriteLedgerEntries)
        {
            errorMsg = "nDataEntriesHigh larger than max write ledger entries";
        }
        // Wasm + instance + data entry reads
        else if (cfg.nDataEntriesHigh + 2 > sorobanCfg.mTxMaxReadLedgerEntries)
        {
            errorMsg = "nDataEntriesHigh larger than max read ledger entries";
        }
        else if (cfg.nDataEntriesHigh * cfg.kiloBytesPerDataEntryHigh * 1024 >
                 sorobanCfg.mTxMaxWriteBytes)
        {
            errorMsg = "TxMaxWriteBytes too small for configuration";
        }
        // Check if we have enough read bytes, using 1'200 as a rough estimate
        // of Wasm size
        else if (cfg.nDataEntriesHigh * cfg.kiloBytesPerDataEntryHigh * 1024 +
                     1'200 >
                 sorobanCfg.mTxMaxReadBytes)
        {
            errorMsg = "TxMaxReadBytes too small for configuration";
        }
    }

    if (errorMsg)
    {
        CLOG_ERROR(LoadGen, "{}", *errorMsg);
        mLoadgenFail.Mark();
        reset();
        return;
    }

    // First time calling tx load generation
    if (!mStarted)
    {
        if (cfg.mode != LoadGenMode::CREATE)
        {
            // Mark all accounts "available" as source accounts
            for (auto i = 0u; i < cfg.nAccounts; i++)
            {
                mAccountsAvailable.insert(i + cfg.offset);
            }
        }

        if (cfg.mode == LoadGenMode::SOROBAN_INVOKE_SETUP)
        {
            // Check if we have already deployed some instances in a previous
            // loadgen run and update nAccounts accordingly
            cfg.nAccounts = mContractInstanceKeys.size() > cfg.nAccounts
                                ? 0
                                : cfg.nAccounts - mContractInstanceKeys.size();
        }
        else if (cfg.mode == LoadGenMode::SOROBAN_INVOKE)
        {
            releaseAssert(mContractInstances.empty());
            releaseAssert(mCodeKey);
            releaseAssert(mAccountsAvailable.size() >= cfg.nAccounts);
            releaseAssert(mContractInstanceKeys.size() >= cfg.nAccounts);

            // assign a unique contract instance to each accountID
            auto accountIter = mAccountsAvailable.begin();
            auto instanceKeyIter = mContractInstanceKeys.begin();
            for (size_t i = 0; i < cfg.nAccounts; ++i)
            {
                ContractInstance instance;
                instance.readOnlyKeys.emplace_back(*mCodeKey);
                instance.readOnlyKeys.emplace_back(*instanceKeyIter);
                instance.contractID = instanceKeyIter->contractData().contract;
                mContractInstances.emplace(*accountIter, instance);
                ++accountIter;
                ++instanceKeyIter;
            }
        }
    }

    if (!mLoadTimer)
    {
        mLoadTimer = std::make_unique<VirtualTimer>(mApp.getClock());
    }

    mStarted = true;

    if (mApp.getState() == Application::APP_SYNCED_STATE)
    {
        mLoadTimer->expires_from_now(std::chrono::milliseconds(STEP_MSECS));
        mLoadTimer->async_wait([this, cfg]() { this->generateLoad(cfg); },
                               &VirtualTimer::onFailureNoop);
    }
    else
    {
        CLOG_WARNING(
            LoadGen,
            "Application is not in sync, load generation inhibited. State {}",
            mApp.getStateHuman());
        mLoadTimer->expires_from_now(std::chrono::seconds(10));
        mLoadTimer->async_wait(
            [this, cfg]() { this->scheduleLoadGeneration(cfg); },
            &VirtualTimer::onFailureNoop);
    }
}

// Generate one "step" worth of load (assuming 1 step per STEP_MSECS) at a
// given target number of accounts and txs, and a given target tx/s rate.
// If work remains after the current step, call scheduleLoadGeneration()
// with the remainder.
void
LoadGenerator::generateLoad(GeneratedLoadConfig cfg)
{
    ZoneScoped;
    bool isSetup = cfg.mode == LoadGenMode::CREATE ||
                   cfg.mode == LoadGenMode::SOROBAN_INVOKE_SETUP;
    if (!mStartTime)
    {
        mStartTime =
            std::make_unique<VirtualClock::time_point>(mApp.getClock().now());
    }

    createRootAccount();

    // Finish if no more txs need to be created.
    if ((isSetup && cfg.nAccounts == 0) || (!isSetup && cfg.nTxs == 0))
    {
        // Done submitting the load, now ensure it propagates to the DB.
        if (!isSetup && cfg.skipLowFeeTxs)
        {
            // skipLowFeeTxs allows triggering tx queue limiter, which makes it
            // hard to track the final seq nums. Hence just wait
            // unconditionally.
            waitTillCompleteWithoutChecks();
        }
        else
        {
            waitTillComplete(cfg.mode == LoadGenMode::CREATE);
        }
        return;
    }

    updateMinBalance();
    if (cfg.txRate == 0)
    {
        cfg.txRate = 1;
    }

    auto txPerStep = getTxPerStep(cfg.txRate, cfg.spikeInterval, cfg.spikeSize);
    if (cfg.mode == LoadGenMode::CREATE)
    {
        // Limit creation to the number of accounts we have. This is only the
        // case at the very beginning, when only root account is available for
        // account creation
        size_t expectedSize =
            mInitialAccountsCreated ? mAccountsAvailable.size() : 1;
        txPerStep = std::min<int64_t>(txPerStep, expectedSize);
    }
    auto& submitTimer =
        mApp.getMetrics().NewTimer({"loadgen", "step", "submit"});
    auto submitScope = submitTimer.TimeScope();

    uint64_t now = mApp.timeNow();
    // Cleaning up accounts every second, so we don't call potentially expensive
    // cleanup function too often
    if (now != mLastSecond)
    {
        cleanupAccounts();
    }

    uint32_t ledgerNum = mApp.getLedgerManager().getLastClosedLedgerNum() + 1;

    for (int64_t i = 0; i < txPerStep; ++i)
    {
        if (cfg.mode == LoadGenMode::CREATE)
        {
            cfg.nAccounts =
                submitCreationTx(cfg.nAccounts, cfg.offset, ledgerNum);
        }
        else if (cfg.mode == LoadGenMode::SOROBAN_INVOKE_SETUP)
        {
            cfg.nAccounts = submitSorobanPrepareInvokeTX(
                cfg.nAccounts, ledgerNum,
                generateFee(cfg.maxGeneratedFeeRate, mApp, /* opsCnt */ 1));
        }
        else
        {
            if (mAccountsAvailable.empty())
            {
                CLOG_WARNING(
                    LoadGen,
                    "Load generation failed: no more accounts available");
                mLoadgenFail.Mark();
                reset();
                return;
            }

            uint64_t sourceAccountId = getNextAvailableAccount();

            std::function<
                std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>()>
                generateTx;

            switch (cfg.mode)
            {
            case LoadGenMode::CREATE:
                releaseAssert(false);
                break;
            case LoadGenMode::PAY:
                generateTx = [&]() {
                    return paymentTransaction(cfg.nAccounts, cfg.offset,
                                              ledgerNum, sourceAccountId, 1,
                                              cfg.maxGeneratedFeeRate);
                };
                break;
            case LoadGenMode::PRETEND:
            {
                auto opCount = chooseOpCount(mApp.getConfig());
                generateTx = [&, opCount]() {
                    return pretendTransaction(cfg.nAccounts, cfg.offset,
                                              ledgerNum, sourceAccountId,
                                              opCount, cfg.maxGeneratedFeeRate);
                };
            }
            break;
            case LoadGenMode::MIXED_TXS:
            {
                auto opCount = chooseOpCount(mApp.getConfig());
                bool isDex = rand_uniform<uint32_t>(1, 100) <= cfg.dexTxPercent;
                generateTx = [&, opCount, isDex]() {
                    if (isDex)
                    {
                        return manageOfferTransaction(ledgerNum,
                                                      sourceAccountId, opCount,
                                                      cfg.maxGeneratedFeeRate);
                    }
                    else
                    {
                        return paymentTransaction(
                            cfg.nAccounts, cfg.offset, ledgerNum,
                            sourceAccountId, opCount, cfg.maxGeneratedFeeRate);
                    }
                };
            }
            break;
            case LoadGenMode::SOROBAN_UPLOAD:
            {
                generateTx = [&]() {
                    SorobanResources resources;
                    uint32_t wasmSize{};
                    {
                        Resource maxPerTx =
                            mApp.getLedgerManager()
                                .maxSorobanTransactionResources();

                        resources.instructions = rand_uniform<uint32_t>(
                            1, static_cast<uint32>(maxPerTx.getVal(
                                   Resource::Type::INSTRUCTIONS)));
                        wasmSize = rand_uniform<uint32_t>(
                            1, mApp.getLedgerManager()
                                   .getSorobanNetworkConfig()
                                   .maxContractSizeBytes());
                        resources.readBytes = rand_uniform<uint32_t>(
                            1, static_cast<uint32>(maxPerTx.getVal(
                                   Resource::Type::READ_BYTES)));
                        resources.writeBytes = rand_uniform<uint32_t>(
                            // Allocate at least enough write bytes to write the
                            // whole Wasm plus the 40 bytes of the key.
                            wasmSize + 40, static_cast<uint32>(maxPerTx.getVal(
                                               Resource::Type::WRITE_BYTES)));

                        auto writeKeys = LedgerTestUtils::
                            generateUniqueValidSorobanLedgerEntryKeys(
                                rand_uniform<uint32_t>(
                                    0, static_cast<uint32>(
                                           maxPerTx.getVal(
                                               Resource::Type::
                                                   WRITE_LEDGER_ENTRIES) -
                                           1)));

                        for (auto const& key : writeKeys)
                        {
                            resources.footprint.readWrite.emplace_back(key);
                        }

                        auto readKeys = LedgerTestUtils::
                            generateUniqueValidSorobanLedgerEntryKeys(
                                rand_uniform<uint32_t>(
                                    0, static_cast<uint32>(
                                           maxPerTx.getVal(
                                               Resource::Type::
                                                   READ_LEDGER_ENTRIES) -
                                           writeKeys.size() - 1)));

                        for (auto const& key : readKeys)
                        {
                            resources.footprint.readOnly.emplace_back(key);
                        }
                    }

                    return sorobanRandomWasmTransaction(
                        ledgerNum, sourceAccountId, resources, wasmSize,
                        generateFee(cfg.maxGeneratedFeeRate, mApp,
                                    /* opsCnt */ 1));
                };
            }
            break;
            case LoadGenMode::SOROBAN_INVOKE_SETUP:
                releaseAssert(false);
                break;
            case LoadGenMode::SOROBAN_INVOKE:
                generateTx = [&]() {
                    return invokeSorobanLoadTransaction(ledgerNum,
                                                        sourceAccountId, cfg);
                };
                break;
            }

            if (submitTx(cfg, generateTx))
            {
                --cfg.nTxs;
            }
            else if (mFailed)
            {
                break;
            }
        }
        if (cfg.nAccounts == 0 || (!isSetup && cfg.nTxs == 0))
        {
            // Nothing to do for the rest of the step
            break;
        }
    }

    auto submit = submitScope.Stop();

    now = mApp.timeNow();

    // Emit a log message once per second.
    if (now != mLastSecond)
    {
        logProgress(submit, cfg.mode, cfg.nAccounts, cfg.nTxs, cfg.txRate);
    }

    mLastSecond = now;
    mTotalSubmitted += txPerStep;
    scheduleLoadGeneration(cfg);
}

uint32_t
LoadGenerator::submitSorobanPrepareInvokeTX(uint32_t nAccounts,
                                            uint32_t ledgerNum,
                                            uint32_t inclusionFee)
{
    TestAccountPtr from;
    TransactionFramePtr tx;
    bool isUpload = false;

    // Check if entry has been applied
    if (mPendingCodeKey.has_value())
    {
        LedgerTxn ltx(mApp.getLedgerTxnRoot());
        if (auto ltxe = ltx.loadWithoutRecord(*mPendingCodeKey); ltxe)
        {
            // Entry has been applied,
            mCodeKey = mPendingCodeKey;
            mCodeSize = xdr::xdr_size(ltxe.current());
            mPendingCodeKey.reset();
        }
        else
        {
            // Still waiting for wasm to be applied, exit early
            return nAccounts;
        }
    }

    uint64_t sourceAccountId = getNextAvailableAccount();

    // First deploy wasm
    if (!mCodeKey.has_value())
    {
        isUpload = true;
        std::tie(from, tx) =
            uploadWasmTransaction(ledgerNum, sourceAccountId, inclusionFee);
    }
    // Next deploy instances
    else
    {
        std::tie(from, tx) =
            createContractTransaction(ledgerNum, sourceAccountId, inclusionFee);
    }

    TransactionResultCode code;
    TransactionQueue::AddResult status;
    bool createDuplicate = false;
    uint32_t numTries = 0;

    while ((status = execute(tx, LoadGenMode::CREATE, code)) !=
           TransactionQueue::AddResult::ADD_STATUS_PENDING)
    {
        // Ignore duplicate transactions, simply continue generating load
        if (status == TransactionQueue::AddResult::ADD_STATUS_DUPLICATE)
        {
            createDuplicate = true;
            break;
        }

        if (++numTries >= TX_SUBMIT_MAX_TRIES ||
            status != TransactionQueue::AddResult::ADD_STATUS_ERROR)
        {
            // Failed to submit the step of load
            mFailed = true;
            return 0;
        }

        // In case of bad seqnum, attempt refreshing it from the DB
        maybeHandleFailedTx(tx, from, status, code);
    }

    // We deploy one shared wasm and an instance per account, so don't decrement
    // accounts if we deployed the shared wasm
    if (!createDuplicate && !isUpload)
    {
        --nAccounts;
    }

    return nAccounts;
}

uint32_t
LoadGenerator::submitCreationTx(uint32_t nAccounts, uint32_t offset,
                                uint32_t ledgerNum)
{
    uint32_t numToProcess =
        nAccounts < MAX_OPS_PER_TX ? nAccounts : MAX_OPS_PER_TX;
    TestAccountPtr from;
    TransactionFramePtr tx;
    std::tie(from, tx) =
        creationTransaction(mAccounts.size() + offset, numToProcess, ledgerNum);
    TransactionResultCode code;
    TransactionQueue::AddResult status;
    bool createDuplicate = false;
    uint32_t numTries = 0;

    while ((status = execute(tx, LoadGenMode::CREATE, code)) !=
           TransactionQueue::AddResult::ADD_STATUS_PENDING)
    {
        // Ignore duplicate transactions, simply continue generating load
        if (status == TransactionQueue::AddResult::ADD_STATUS_DUPLICATE)
        {
            createDuplicate = true;
            break;
        }

        if (++numTries >= TX_SUBMIT_MAX_TRIES ||
            status != TransactionQueue::AddResult::ADD_STATUS_ERROR)
        {
            // Failed to submit the step of load
            mFailed = true;
            return 0;
        }

        // In case of bad seqnum, attempt refreshing it from the DB
        maybeHandleFailedTx(tx, from, status, code);
    }

    if (!createDuplicate)
    {
        nAccounts -= numToProcess;
    }

    return nAccounts;
}

bool
LoadGenerator::submitTx(GeneratedLoadConfig const& cfg,
                        std::function<std::pair<LoadGenerator::TestAccountPtr,
                                                TransactionFramePtr>()>
                            generateTx)
{
    auto [from, tx] = generateTx();

    TransactionResultCode code;
    TransactionQueue::AddResult status;
    uint32_t numTries = 0;

    while ((status = execute(tx, cfg.mode, code)) !=
           TransactionQueue::AddResult::ADD_STATUS_PENDING)
    {

        if (cfg.skipLowFeeTxs &&
            (status ==
                 TransactionQueue::AddResult::ADD_STATUS_TRY_AGAIN_LATER ||
             (status == TransactionQueue::AddResult::ADD_STATUS_ERROR &&
              code == txINSUFFICIENT_FEE)))
        {
            // Rollback the seq num of the test account as we regenerate the
            // transaction.
            from->setSequenceNumber(from->getLastSequenceNumber() - 1);
            CLOG_INFO(LoadGen, "skipped low fee tx with fee {}",
                      tx->getInclusionFee());
            return false;
        }
        if (++numTries >= TX_SUBMIT_MAX_TRIES ||
            status != TransactionQueue::AddResult::ADD_STATUS_ERROR)
        {
            mFailed = true;
            return false;
        }

        // In case of bad seqnum, attempt refreshing it from the DB
        maybeHandleFailedTx(tx, from, status, code); // Update seq num

        // Regenerate a new payment tx
        std::tie(from, tx) = generateTx();
    }

    return true;
}

uint64_t
LoadGenerator::getNextAvailableAccount()
{
    releaseAssert(!mAccountsAvailable.empty());

    auto sourceAccountIdx =
        rand_uniform<uint64_t>(0, mAccountsAvailable.size() - 1);
    auto it = mAccountsAvailable.begin();
    std::advance(it, sourceAccountIdx);
    uint64_t sourceAccountId = *it;
    mAccountsAvailable.erase(it);
    releaseAssert(mAccountsInUse.insert(sourceAccountId).second);
    return sourceAccountId;
}

void
LoadGenerator::logProgress(std::chrono::nanoseconds submitTimer,
                           LoadGenMode mode, uint32_t nAccounts, uint32_t nTxs,
                           uint32_t txRate)
{
    using namespace std::chrono;

    auto& m = mApp.getMetrics();
    auto& applyTx = m.NewTimer({"ledger", "transaction", "apply"});
    auto& applyOp = m.NewTimer({"ledger", "operation", "apply"});

    auto submitSteps = duration_cast<milliseconds>(submitTimer).count();

    auto remainingTxCount =
        (mode == LoadGenMode::CREATE) ? nAccounts / MAX_OPS_PER_TX : nTxs;
    auto etaSecs = (uint32_t)(((double)remainingTxCount) /
                              max<double>(1, applyTx.one_minute_rate()));

    auto etaHours = etaSecs / 3600;
    auto etaMins = etaSecs % 60;

    CLOG_INFO(LoadGen,
              "Tx/s: {} target, {}tx/{}op actual (1m EWMA). Pending: {} "
              "accounts, {} txs. ETA: {}h{}m",
              txRate, applyTx.one_minute_rate(), applyOp.one_minute_rate(),
              nAccounts, nTxs, etaHours, etaMins);

    CLOG_DEBUG(LoadGen, "Step timing: {}ms submit.", submitSteps);

    TxMetrics txm(mApp.getMetrics());
    txm.report();
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::creationTransaction(uint64_t startAccount, uint64_t numItems,
                                   uint32_t ledgerNum)
{
    TestAccountPtr sourceAcc =
        mInitialAccountsCreated
            ? findAccount(getNextAvailableAccount(), ledgerNum)
            : mRoot;
    vector<Operation> creationOps = createAccounts(
        startAccount, numItems, ledgerNum, !mInitialAccountsCreated);
    mInitialAccountsCreated = true;
    return std::make_pair(sourceAcc, createTransactionFramePtr(
                                         sourceAcc, creationOps,
                                         LoadGenMode::CREATE, std::nullopt));
}

void
LoadGenerator::updateMinBalance()
{
    auto b = mApp.getLedgerManager().getLastMinBalance(0);
    if (b > mMinBalance)
    {
        mMinBalance = b;
    }
}

std::vector<Operation>
LoadGenerator::createAccounts(uint64_t start, uint64_t count,
                              uint32_t ledgerNum, bool initialAccounts)
{
    vector<Operation> ops;
    SequenceNumber sn = static_cast<SequenceNumber>(ledgerNum) << 32;
    auto balance = initialAccounts ? mMinBalance * 10000000 : mMinBalance * 100;
    for (uint64_t i = start; i < start + count; i++)
    {
        auto name = "TestAccount-" + to_string(i);
        auto account = TestAccount{mApp, txtest::getAccount(name.c_str()), sn};
        ops.push_back(txtest::createAccount(account.getPublicKey(), balance));

        // Cache newly created account
        auto acc = make_shared<TestAccount>(account);
        mAccounts.emplace(i, acc);
        if (initialAccounts)
        {
            mCreationSourceAccounts.emplace(i, acc);
        }
    }
    return ops;
}

bool
LoadGenerator::loadAccount(TestAccount& account, Application& app)
{
    LedgerTxn ltx(app.getLedgerTxnRoot());
    auto entry = stellar::loadAccount(ltx, account.getPublicKey());
    if (!entry)
    {
        return false;
    }
    account.setSequenceNumber(entry.current().data.account().seqNum);
    return true;
}

bool
LoadGenerator::loadAccount(TestAccountPtr acc, Application& app)
{
    if (acc)
    {
        return loadAccount(*acc, app);
    }
    return false;
}

std::pair<LoadGenerator::TestAccountPtr, LoadGenerator::TestAccountPtr>
LoadGenerator::pickAccountPair(uint32_t numAccounts, uint32_t offset,
                               uint32_t ledgerNum, uint64_t sourceAccountId)
{
    auto sourceAccount = findAccount(sourceAccountId, ledgerNum);
    releaseAssert(
        !mApp.getHerder().sourceAccountPending(sourceAccount->getPublicKey()));

    auto destAccountId = rand_uniform<uint64_t>(0, numAccounts - 1) + offset;

    auto destAccount = findAccount(destAccountId, ledgerNum);

    CLOG_DEBUG(LoadGen, "Generated pair for payment tx - {} and {}",
               sourceAccountId, destAccountId);
    return std::pair<TestAccountPtr, TestAccountPtr>(sourceAccount,
                                                     destAccount);
}

LoadGenerator::TestAccountPtr
LoadGenerator::findAccount(uint64_t accountId, uint32_t ledgerNum)
{
    // Load account and cache it.
    TestAccountPtr newAccountPtr;

    auto res = mAccounts.find(accountId);
    if (res == mAccounts.end())
    {
        SequenceNumber sn = static_cast<SequenceNumber>(ledgerNum) << 32;
        auto name = "TestAccount-" + std::to_string(accountId);
        newAccountPtr =
            std::make_shared<TestAccount>(mApp, txtest::getAccount(name), sn);

        if (!loadAccount(newAccountPtr, mApp))
        {
            throw std::runtime_error(
                fmt::format("Account {0} must exist in the DB.", accountId));
        }
        mAccounts.insert(
            std::pair<uint64_t, TestAccountPtr>(accountId, newAccountPtr));
    }
    else
    {
        newAccountPtr = res->second;
    }

    return newAccountPtr;
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::paymentTransaction(uint32_t numAccounts, uint32_t offset,
                                  uint32_t ledgerNum, uint64_t sourceAccount,
                                  uint32_t opCount,
                                  std::optional<uint32_t> maxGeneratedFeeRate)
{
    TestAccountPtr to, from;
    uint64_t amount = 1;
    std::tie(from, to) =
        pickAccountPair(numAccounts, offset, ledgerNum, sourceAccount);
    vector<Operation> paymentOps;
    paymentOps.reserve(opCount);
    for (uint32_t i = 0; i < opCount; ++i)
    {
        paymentOps.emplace_back(txtest::payment(to->getPublicKey(), amount));
    }

    return std::make_pair(from, createTransactionFramePtr(from, paymentOps,
                                                          LoadGenMode::PAY,
                                                          maxGeneratedFeeRate));
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::manageOfferTransaction(
    uint32_t ledgerNum, uint64_t accountId, uint32_t opCount,
    std::optional<uint32_t> maxGeneratedFeeRate)
{
    auto account = findAccount(accountId, ledgerNum);
    Asset selling(ASSET_TYPE_NATIVE);
    Asset buying(ASSET_TYPE_CREDIT_ALPHANUM4);
    strToAssetCode(buying.alphaNum4().assetCode, "USD");
    vector<Operation> ops;
    for (uint32_t i = 0; i < opCount; ++i)
    {
        ops.emplace_back(txtest::manageBuyOffer(
            rand_uniform<int64_t>(1, 10000000), selling, buying,
            Price{rand_uniform<int32_t>(1, 100), rand_uniform<int32_t>(1, 100)},
            100));
    }
    return std::make_pair(
        account, createTransactionFramePtr(account, ops, LoadGenMode::MIXED_TXS,
                                           maxGeneratedFeeRate));
}

static void
increaseOpSize(Operation& op, uint32_t increaseUpToBytes)
{
    if (increaseUpToBytes == 0)
    {
        return;
    }

    SorobanAuthorizationEntry auth;
    auth.credentials.type(SOROBAN_CREDENTIALS_SOURCE_ACCOUNT);
    auth.rootInvocation.function.type(
        SOROBAN_AUTHORIZED_FUNCTION_TYPE_CONTRACT_FN);
    SCVal val(SCV_BYTES);

    auto const overheadBytes = xdr::xdr_size(auth) + xdr::xdr_size(val);
    if (overheadBytes > increaseUpToBytes)
    {
        increaseUpToBytes = 0;
    }
    else
    {
        increaseUpToBytes -= overheadBytes;
    }

    val.bytes().resize(increaseUpToBytes);
    auth.rootInvocation.function.contractFn().args = {val};
    op.body.invokeHostFunctionOp().auth = {auth};
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::uploadWasmTransaction(uint32_t ledgerNum, uint64_t accountId,
                                     uint32_t inclusionFee)
{
    auto wasm = rust_bridge::get_test_wasm_loadgen();
    auto account = findAccount(accountId, ledgerNum);

    SorobanResources uploadResources{};
    uploadResources.instructions = 200'000 + (wasm.data.size() * 6000);
    uploadResources.readBytes = 5000;
    uploadResources.writeBytes = 5000;

    Operation uploadOp;
    uploadOp.body.type(INVOKE_HOST_FUNCTION);
    auto& uploadHF = uploadOp.body.invokeHostFunctionOp().hostFunction;
    uploadHF.type(HOST_FUNCTION_TYPE_UPLOAD_CONTRACT_WASM);
    uploadHF.wasm().assign(wasm.data.begin(), wasm.data.end());

    LedgerKey contractCodeLedgerKey;
    contractCodeLedgerKey.type(CONTRACT_CODE);
    contractCodeLedgerKey.contractCode().hash = sha256(uploadHF.wasm());
    uploadResources.footprint.readWrite = {contractCodeLedgerKey};

    int64_t resourceFee =
        sorobanResourceFee(mApp, uploadResources, 5000 + wasm.data.size(), 100);
    resourceFee += 1'000'000;
    auto tx = std::dynamic_pointer_cast<TransactionFrame>(
        sorobanTransactionFrameFromOps(mApp.getNetworkID(), *account,
                                       {uploadOp}, {}, uploadResources,
                                       inclusionFee, resourceFee));
    mPendingCodeKey = contractCodeLedgerKey;
    return std::make_pair(account, tx);
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::createContractTransaction(uint32_t ledgerNum, uint64_t accountId,
                                         uint32_t inclusionFee)
{
    releaseAssert(mCodeKey);

    auto account = findAccount(accountId, ledgerNum);
    SorobanResources createResources{};
    createResources.instructions = 200'000;
    createResources.readBytes = 5000;
    createResources.writeBytes = 5000;

    SCVal scContractSourceRefKey(SCValType::SCV_LEDGER_KEY_CONTRACT_INSTANCE);
    auto salt = sha256(std::to_string(mContractInstanceKeys.size()));
    auto [createOp, contractID] =
        createSorobanCreateOp(mApp, createResources, *mCodeKey, *account,
                              scContractSourceRefKey, salt);

    auto const& instanceLk = createResources.footprint.readWrite.back();
    mContractInstanceKeys.emplace(instanceLk);

    auto resourceFee = sorobanResourceFee(mApp, createResources, 1000, 40);
    resourceFee += 1'000'000;
    auto tx = std::dynamic_pointer_cast<TransactionFrame>(
        sorobanTransactionFrameFromOps(mApp.getNetworkID(), *account,
                                       {createOp}, {}, createResources,
                                       inclusionFee, resourceFee));

    return std::make_pair(account, tx);
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::invokeSorobanLoadTransaction(uint32_t ledgerNum,
                                            uint64_t accountId,
                                            GeneratedLoadConfig const& cfg)
{
    auto account = findAccount(accountId, ledgerNum);
    auto instanceIter = mContractInstances.find(accountId);
    releaseAssert(instanceIter != mContractInstances.end());
    auto const& instance = instanceIter->second;

    auto const& networkCfg = mApp.getLedgerManager().getSorobanNetworkConfig();

    // Approximate instruction measurements from loadgen contract. While the
    // guest and host cycle counts are exact, and we can predict the cost of
    // the guest and host loops correctly, it is difficult to estimate the
    // CPU cost of storage given that the number and size of keys is variable.
    // baseInstructionCount is a rough estimate for storage cost, but might be
    // too small if a given invocation writes many or large entries.
    // This means some TXs will fail due to exceeding resource
    // limitations. However these should fail at apply time, so will still
    // generate siginificant load
    uint64_t const baseInstructionCount = 3'000'000;
    uint64_t const instructionsPerGuestCycle = 120;
    uint64_t const instructionsPerHostCycle = 2355;

    // Pick random number of cycles between bounds, respecting network limits
    uint64_t maxInstructions =
        networkCfg.mTxMaxInstructions - baseInstructionCount;
    maxInstructions = std::min(maxInstructions, cfg.instructionsHigh);
    uint64_t lowInstructions = std::min(maxInstructions, cfg.instructionsLow);
    uint64_t targetInstructions =
        rand_uniform<uint64_t>(lowInstructions, maxInstructions);

    // Randomly select a number of guest cycles
    uint64_t guestCyclesMax = targetInstructions / instructionsPerGuestCycle;
    uint64_t guestCycles = rand_uniform<uint64_t>(0, guestCyclesMax);

    // Rest of instructions consumed by host cycles
    targetInstructions -= guestCycles * instructionsPerGuestCycle;
    uint64_t hostCycles = targetInstructions / instructionsPerHostCycle;

    SorobanResources resources;
    resources.footprint.readOnly = instance.readOnlyKeys;

    // Must always read wasm and instance
    releaseAssert(networkCfg.mTxMaxReadLedgerEntries > 1);
    auto maxEntries = networkCfg.mTxMaxReadLedgerEntries - 2;
    maxEntries = std::min(maxEntries, cfg.nDataEntriesHigh);
    auto minEntries = std::min(maxEntries, cfg.nDataEntriesLow);
    auto numEntries = rand_uniform<uint32_t>(minEntries, maxEntries);
    for (uint32_t i = 0; i < numEntries; ++i)
    {
        auto lk = contractDataKey(instance.contractID, makeU32(i),
                                  ContractDataDurability::PERSISTENT);
        resources.footprint.readWrite.emplace_back(lk);
    }

    uint32_t maxKiloBytesPerEntry =
        (networkCfg.mTxMaxReadBytes - mCodeSize) / numEntries / 1024;
    maxKiloBytesPerEntry =
        std::min(maxKiloBytesPerEntry, cfg.kiloBytesPerDataEntryHigh);
    uint32_t minKiloBytesPerEntry =
        std::min(maxKiloBytesPerEntry, cfg.kiloBytesPerDataEntryLow);
    auto kiloBytesPerEntry =
        rand_uniform<uint32_t>(minKiloBytesPerEntry, maxKiloBytesPerEntry);

    auto guestCyclesU64 = makeU64(guestCycles);
    auto hostCyclesU64 = makeU64(hostCycles);
    auto numEntriesU32 = makeU32(numEntries);
    auto kiloBytesPerEntryU32 = makeU32(kiloBytesPerEntry);

    Operation op;
    op.body.type(INVOKE_HOST_FUNCTION);
    auto& ihf = op.body.invokeHostFunctionOp().hostFunction;
    ihf.type(HOST_FUNCTION_TYPE_INVOKE_CONTRACT);
    ihf.invokeContract().contractAddress = instance.contractID;
    ihf.invokeContract().functionName = "do_work";
    ihf.invokeContract().args = {guestCyclesU64, hostCyclesU64, numEntriesU32,
                                 kiloBytesPerEntryU32};

    // We don't have a good way of knowing how many bytes we will have to read,
    // so use max. Due to this we will have to change ledger limits, so might as
    // well set all resources to max
    resources.instructions = networkCfg.mTxMaxInstructions;
    resources.readBytes = networkCfg.mTxMaxReadBytes;
    resources.writeBytes = networkCfg.mTxMaxWriteBytes;

    // Approximate TX size before padding and footprint, slightly over estimated
    // so we stay below limits, plus footprint size
    int32_t const txOverheadBytes = 260 + xdr::xdr_size(resources);
    int32_t paddingBytesLow = txOverheadBytes > cfg.txSizeBytesLow
                                  ? 0
                                  : cfg.txSizeBytesLow - txOverheadBytes;
    int32_t paddingBytesHigh = txOverheadBytes > cfg.txSizeBytesHigh
                                   ? 0
                                   : cfg.txSizeBytesHigh - txOverheadBytes;
    auto paddingBytes =
        rand_uniform<int32_t>(paddingBytesLow, paddingBytesHigh);
    increaseOpSize(op, paddingBytes);

    auto resourceFee =
        sorobanResourceFee(mApp, resources, txOverheadBytes + paddingBytes, 40);
    resourceFee += 1'000'000;

    auto tx = std::dynamic_pointer_cast<TransactionFrame>(
        sorobanTransactionFrameFromOps(
            mApp.getNetworkID(), *account, {op}, {}, resources,
            generateFee(cfg.maxGeneratedFeeRate, mApp,
                        /* opsCnt */ 1),
            resourceFee));

    return std::make_pair(account, tx);
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::sorobanRandomWasmTransaction(uint32_t ledgerNum,
                                            uint64_t accountId,
                                            SorobanResources resources,
                                            size_t wasmSize,
                                            uint32_t inclusionFee)
{
    auto account = findAccount(accountId, ledgerNum);
    Operation uploadOp =
        createUploadWasmOperation(static_cast<uint32>(wasmSize));
    LedgerKey contractCodeLedgerKey;
    contractCodeLedgerKey.type(CONTRACT_CODE);
    contractCodeLedgerKey.contractCode().hash =
        sha256(uploadOp.body.invokeHostFunctionOp().hostFunction.wasm());
    resources.footprint.readWrite.push_back(contractCodeLedgerKey);

    int64_t resourceFee =
        sorobanResourceFee(mApp, resources, 5000 + wasmSize, 100);
    // Roughly cover the rent fee.
    resourceFee += 100000;
    auto tx = std::dynamic_pointer_cast<TransactionFrame>(
        sorobanTransactionFrameFromOps(mApp.getNetworkID(), *account,
                                       {uploadOp}, {}, resources, inclusionFee,
                                       resourceFee));
    return std::make_pair(account, tx);
}

std::pair<LoadGenerator::TestAccountPtr, TransactionFramePtr>
LoadGenerator::pretendTransaction(uint32_t numAccounts, uint32_t offset,
                                  uint32_t ledgerNum, uint64_t sourceAccount,
                                  uint32_t opCount,
                                  std::optional<uint32_t> maxGeneratedFeeRate)
{
    vector<Operation> ops;
    ops.reserve(opCount);
    auto acc = findAccount(sourceAccount, ledgerNum);
    for (uint32 i = 0; i < opCount; i++)
    {
        auto args = SetOptionsArguments{};

        // We make SetOptionsOps such that we end up
        // with a n-op transaction that is exactly 100n + 240 bytes.
        args.inflationDest = std::make_optional<AccountID>(acc->getPublicKey());
        args.homeDomain = std::make_optional<std::string>(std::string(16, '*'));
        if (i == 0)
        {
            // The first operation needs to be bigger to achieve
            // 100n + 240 bytes.
            args.homeDomain->append(std::string(8, '*'));
            args.signer = std::make_optional<Signer>(Signer{});
        }
        ops.push_back(txtest::setOptions(args));
    }
    return std::make_pair(acc, createTransactionFramePtr(acc, ops,
                                                         LoadGenMode::PRETEND,
                                                         maxGeneratedFeeRate));
}

void
LoadGenerator::maybeHandleFailedTx(TransactionFramePtr tx,
                                   TestAccountPtr sourceAccount,
                                   TransactionQueue::AddResult status,
                                   TransactionResultCode code)
{
    // Note that if transaction is a DUPLICATE, its sequence number is
    // incremented on the next call to execute.
    if (status == TransactionQueue::AddResult::ADD_STATUS_ERROR &&
        code == txBAD_SEQ)
    {
        auto txQueueSeqNum =
            tx->isSoroban()
                ? mApp.getHerder()
                      .getSorobanTransactionQueue()
                      .getInQueueSeqNum(sourceAccount->getPublicKey())
                : mApp.getHerder().getTransactionQueue().getInQueueSeqNum(
                      sourceAccount->getPublicKey());
        if (txQueueSeqNum)
        {
            sourceAccount->setSequenceNumber(*txQueueSeqNum);
            return;
        }
        if (!loadAccount(sourceAccount, mApp))
        {
            CLOG_ERROR(LoadGen, "Unable to reload account {}",
                       sourceAccount->getAccountId());
        }
    }
}

std::vector<LoadGenerator::TestAccountPtr>
LoadGenerator::checkAccountSynced(Application& app, bool isCreate)
{
    std::vector<TestAccountPtr> result;
    for (auto const& acc : mAccounts)
    {
        TestAccountPtr account = acc.second;
        auto accountFromDB = *account;

        auto reloadRes = loadAccount(accountFromDB, app);
        // For account creation, reload accounts from the DB
        // For payments, ensure that the sequence number matches expected
        // seqnum. Timeout after 20 ledgers.
        if (isCreate)
        {
            if (!reloadRes)
            {
                CLOG_TRACE(LoadGen, "Account {} is not created yet!",
                           account->getAccountId());
                result.push_back(account);
            }
        }
        else if (!reloadRes)
        {
            auto msg =
                fmt::format("Account {} used to submit payment tx could not "
                            "load, DB might be in a corrupted state",
                            account->getAccountId());
            throw std::runtime_error(msg);
        }
        else if (account->getLastSequenceNumber() !=
                 accountFromDB.getLastSequenceNumber())
        {
            CLOG_TRACE(LoadGen,
                       "Account {} is at sequence num {}, but the DB is at  {}",
                       account->getAccountId(),
                       account->getLastSequenceNumber(),
                       accountFromDB.getLastSequenceNumber());
            result.push_back(account);
        }
    }
    return result;
}

void
LoadGenerator::waitTillComplete(bool isCreate)
{
    if (!mLoadTimer)
    {
        mLoadTimer = std::make_unique<VirtualTimer>(mApp.getClock());
    }
    vector<TestAccountPtr> inconsistencies;
    inconsistencies = checkAccountSynced(mApp, isCreate);

    if (inconsistencies.empty())
    {
        CLOG_INFO(LoadGen, "Load generation complete.");
        mLoadgenComplete.Mark();
        reset();
        return;
    }
    else
    {
        if (++mWaitTillCompleteForLedgers >= TIMEOUT_NUM_LEDGERS)
        {
            CLOG_INFO(LoadGen, "Load generation failed.");
            mLoadgenFail.Mark();
            reset();
            return;
        }

        mLoadTimer->expires_from_now(
            mApp.getConfig().getExpectedLedgerCloseTime());
        mLoadTimer->async_wait(
            [this, isCreate]() { this->waitTillComplete(isCreate); },
            &VirtualTimer::onFailureNoop);
    }
}

void
LoadGenerator::waitTillCompleteWithoutChecks()
{
    if (!mLoadTimer)
    {
        mLoadTimer = std::make_unique<VirtualTimer>(mApp.getClock());
    }
    if (++mWaitTillCompleteForLedgers == COMPLETION_TIMEOUT_WITHOUT_CHECKS)
    {
        auto inconsistencies = checkAccountSynced(mApp, /* isCreate */ false);
        CLOG_INFO(LoadGen, "Load generation complete.");
        if (!inconsistencies.empty())
        {
            CLOG_INFO(
                LoadGen,
                "{} account seq nums are not in sync with db; this is expected "
                "for high traffic due to tx queue limiter evictions.",
                inconsistencies.size());
        }
        mLoadgenComplete.Mark();
        reset();
        return;
    }
    mLoadTimer->expires_from_now(mApp.getConfig().getExpectedLedgerCloseTime());
    mLoadTimer->async_wait([this]() { this->waitTillCompleteWithoutChecks(); },
                           &VirtualTimer::onFailureNoop);
}

LoadGenerator::TxMetrics::TxMetrics(medida::MetricsRegistry& m)
    : mAccountCreated(m.NewMeter({"loadgen", "account", "created"}, "account"))
    , mNativePayment(m.NewMeter({"loadgen", "payment", "submitted"}, "op"))
    , mManageOfferOps(m.NewMeter({"loadgen", "manageoffer", "submitted"}, "op"))
    , mPretendOps(m.NewMeter({"loadgen", "pretend", "submitted"}, "op"))
    , mTxnAttempted(m.NewMeter({"loadgen", "txn", "attempted"}, "txn"))
    , mTxnRejected(m.NewMeter({"loadgen", "txn", "rejected"}, "txn"))
    , mTxnBytes(m.NewMeter({"loadgen", "txn", "bytes"}, "txn"))
{
}

void
LoadGenerator::TxMetrics::report()
{
    CLOG_DEBUG(LoadGen,
               "Counts: {} tx, {} rj, {} by, {} ac ({} na, {} pr, {} dex",
               mTxnAttempted.count(), mTxnRejected.count(), mTxnBytes.count(),
               mAccountCreated.count(), mNativePayment.count(),
               mPretendOps.count(), mManageOfferOps.one_minute_rate());

    CLOG_DEBUG(
        LoadGen,
        "Rates/sec (1m EWMA): {} tx, {} rj, {} by, {} ac, {} na, {} pr, {} dex",
        mTxnAttempted.one_minute_rate(), mTxnRejected.one_minute_rate(),
        mTxnBytes.one_minute_rate(), mAccountCreated.one_minute_rate(),
        mNativePayment.one_minute_rate(), mPretendOps.one_minute_rate(),
        mManageOfferOps.one_minute_rate());
}

TransactionFramePtr
LoadGenerator::createTransactionFramePtr(
    TestAccountPtr from, std::vector<Operation> ops, LoadGenMode mode,
    std::optional<uint32_t> maxGeneratedFeeRate)
{

    auto txf = transactionFromOperations(
        mApp, from->getSecretKey(), from->nextSequenceNumber(), ops,
        generateFee(maxGeneratedFeeRate, mApp, ops.size()));
    if (mode == LoadGenMode::PRETEND)
    {
        Memo memo(MEMO_TEXT);
        memo.text() = std::string(28, ' ');
        txbridge::setMemo(txf, memo);

        txbridge::setMinTime(txf, 0);
        txbridge::setMaxTime(txf, UINT64_MAX);
    }

    txbridge::getSignatures(txf).clear();
    txf->addSignature(from->getSecretKey());
    return txf;
}

TransactionQueue::AddResult
LoadGenerator::execute(TransactionFramePtr& txf, LoadGenMode mode,
                       TransactionResultCode& code)
{
    TxMetrics txm(mApp.getMetrics());

    // Record tx metrics.
    switch (mode)
    {
    case LoadGenMode::CREATE:
        txm.mAccountCreated.Mark(txf->getNumOperations());
        break;
    case LoadGenMode::PAY:
        txm.mNativePayment.Mark(txf->getNumOperations());
        break;
    case LoadGenMode::PRETEND:
        txm.mPretendOps.Mark(txf->getNumOperations());
        break;
    case LoadGenMode::MIXED_TXS:
        if (txf->hasDexOperations())
        {
            txm.mManageOfferOps.Mark(txf->getNumOperations());
        }
        else
        {
            txm.mNativePayment.Mark(txf->getNumOperations());
        }
        break;
    }

    txm.mTxnAttempted.Mark();

    StellarMessage msg(txf->toStellarMessage());
    txm.mTxnBytes.Mark(xdr::xdr_argpack_size(msg));

    auto status = mApp.getHerder().recvTransaction(txf, true);
    if (status != TransactionQueue::AddResult::ADD_STATUS_PENDING)
    {
        CLOG_INFO(LoadGen, "tx rejected '{}': ===> {}",
                  TX_STATUS_STRING[static_cast<int>(status)],
                  txf->isSoroban() ? "soroban"
                                   : xdr_to_string(txf->getEnvelope(),
                                                   "TransactionEnvelope"),
                  xdr_to_string(txf->getResult(), "TransactionResult"));
        if (status == TransactionQueue::AddResult::ADD_STATUS_ERROR)
        {
            code = txf->getResultCode();
        }
        txm.mTxnRejected.Mark();
    }
    else
    {
        mApp.getOverlayManager().broadcastMessage(msg, false,
                                                  txf->getFullHash());
    }

    return status;
}

GeneratedLoadConfig
GeneratedLoadConfig::createAccountsLoad(uint32_t nAccounts, uint32_t txRate)
{
    GeneratedLoadConfig cfg;
    cfg.mode = LoadGenMode::CREATE;
    cfg.nAccounts = nAccounts;
    cfg.txRate = txRate;
    return cfg;
}

GeneratedLoadConfig
GeneratedLoadConfig::createSorobanInvokeSetupLoad(uint32_t nAccounts,
                                                  uint32_t txRate)
{
    GeneratedLoadConfig cfg;
    cfg.mode = LoadGenMode::SOROBAN_INVOKE_SETUP;
    cfg.nAccounts = nAccounts;
    cfg.txRate = txRate;
    return cfg;
}

GeneratedLoadConfig
GeneratedLoadConfig::txLoad(LoadGenMode mode, uint32_t nAccounts, uint32_t nTxs,
                            uint32_t txRate, uint32_t offset,
                            std::optional<uint32_t> maxFee)
{
    GeneratedLoadConfig cfg;
    cfg.mode = mode;
    cfg.nAccounts = nAccounts;
    cfg.nTxs = nTxs;
    cfg.txRate = txRate;
    cfg.offset = offset;
    cfg.maxGeneratedFeeRate = maxFee;
    return cfg;
}
}
