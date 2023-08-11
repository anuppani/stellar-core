// Copyright 2023 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "ledger/NetworkConfig.h"
#include "bucket/BucketList.h"
#include "bucket/BucketManager.h"
#include "main/Application.h"
#include "util/ProtocolVersion.h"

namespace stellar
{
namespace
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
void
createConfigSettingEntry(ConfigSettingEntry const& configSetting,
                         AbstractLedgerTxn& ltxRoot)
{
    if (!SorobanNetworkConfig::isValidConfigSettingEntry(configSetting))
    {
        throw std::runtime_error("Invalid configSettingEntry");
    }

    LedgerEntry e;
    e.data.type(CONFIG_SETTING);
    e.data.configSetting() = configSetting;
    LedgerTxn ltx(ltxRoot);
    ltx.create(e);
    ltx.commit();
}

ConfigSettingEntry
initialMaxContractSizeEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_MAX_SIZE_BYTES);

    entry.contractMaxSizeBytes() =
        InitialSorobanNetworkConfig::MAX_CONTRACT_SIZE;

    return entry;
}

ConfigSettingEntry
initialMaxContractDataKeySizeEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_DATA_KEY_SIZE_BYTES);

    entry.contractDataKeySizeBytes() =
        InitialSorobanNetworkConfig::MAX_CONTRACT_DATA_KEY_SIZE_BYTES;

    return entry;
}

ConfigSettingEntry
initialMaxContractDataEntrySizeEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_DATA_ENTRY_SIZE_BYTES);

    entry.contractDataEntrySizeBytes() =
        InitialSorobanNetworkConfig::MAX_CONTRACT_DATA_ENTRY_SIZE_BYTES;

    return entry;
}

ConfigSettingEntry
initialContractComputeSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_COMPUTE_V0);
    auto& e = entry.contractCompute();

    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxInstructions = cfg.TESTING_LEDGER_MAX_INSTRUCTIONS;
    }
    else
    {
        e.ledgerMaxInstructions =
            InitialSorobanNetworkConfig::LEDGER_MAX_INSTRUCTIONS;
    }
    e.txMaxInstructions = InitialSorobanNetworkConfig::TX_MAX_INSTRUCTIONS;
    e.feeRatePerInstructionsIncrement =
        InitialSorobanNetworkConfig::FEE_RATE_PER_INSTRUCTIONS_INCREMENT;
    e.txMemoryLimit = InitialSorobanNetworkConfig::MEMORY_LIMIT;

    return entry;
}

ConfigSettingEntry
initialContractLedgerAccessSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_LEDGER_COST_V0);
    auto& e = entry.contractLedgerCost();

    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxReadLedgerEntries =
            cfg.TESTING_LEDGER_MAX_READ_LEDGER_ENTRIES;
    }
    else
    {
        e.ledgerMaxReadLedgerEntries =
            InitialSorobanNetworkConfig::LEDGER_MAX_READ_LEDGER_ENTRIES;
    }
    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxReadBytes = cfg.TESTING_LEDGER_MAX_READ_BYTES;
    }
    else
    {
        e.ledgerMaxReadBytes =
            InitialSorobanNetworkConfig::LEDGER_MAX_READ_BYTES;
    }
    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxWriteLedgerEntries =
            cfg.TESTING_LEDGER_MAX_WRITE_LEDGER_ENTRIES;
    }
    else
    {
        e.ledgerMaxWriteLedgerEntries =
            InitialSorobanNetworkConfig::LEDGER_MAX_WRITE_LEDGER_ENTRIES;
    }
    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxWriteBytes = cfg.TESTING_LEDGER_MAX_WRITE_BYTES;
    }
    else
    {
        e.ledgerMaxWriteBytes =
            InitialSorobanNetworkConfig::LEDGER_MAX_WRITE_BYTES;
    }
    e.txMaxReadLedgerEntries =
        InitialSorobanNetworkConfig::TX_MAX_READ_LEDGER_ENTRIES;
    e.txMaxReadBytes = InitialSorobanNetworkConfig::TX_MAX_READ_BYTES;
    e.txMaxWriteLedgerEntries =
        InitialSorobanNetworkConfig::TX_MAX_WRITE_LEDGER_ENTRIES;
    e.txMaxWriteBytes = InitialSorobanNetworkConfig::TX_MAX_WRITE_BYTES;
    e.feeReadLedgerEntry = InitialSorobanNetworkConfig::FEE_READ_LEDGER_ENTRY;
    e.feeWriteLedgerEntry = InitialSorobanNetworkConfig::FEE_WRITE_LEDGER_ENTRY;
    e.feeRead1KB = InitialSorobanNetworkConfig::FEE_READ_1KB;
    e.bucketListTargetSizeBytes =
        InitialSorobanNetworkConfig::BUCKET_LIST_TARGET_SIZE_BYTES;
    e.writeFee1KBBucketListLow =
        InitialSorobanNetworkConfig::BUCKET_LIST_FEE_1KB_BUCKET_LIST_LOW;
    e.writeFee1KBBucketListHigh =
        InitialSorobanNetworkConfig::BUCKET_LIST_FEE_1KB_BUCKET_LIST_HIGH;
    e.bucketListWriteFeeGrowthFactor =
        InitialSorobanNetworkConfig::BUCKET_LIST_WRITE_FEE_GROWTH_FACTOR;

    return entry;
}

ConfigSettingEntry
initialContractHistoricalDataSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_HISTORICAL_DATA_V0);
    auto& e = entry.contractHistoricalData();

    e.feeHistorical1KB = InitialSorobanNetworkConfig::FEE_HISTORICAL_1KB;

    return entry;
}

ConfigSettingEntry
initialContractEventsSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_EVENTS_V0);
    auto& e = entry.contractEvents();

    e.txMaxContractEventsSizeBytes =
        InitialSorobanNetworkConfig::TX_MAX_CONTRACT_EVENTS_SIZE_BYTES;
    e.feeContractEvents1KB =
        InitialSorobanNetworkConfig::FEE_CONTRACT_EVENTS_SIZE_1KB;

    return entry;
}

ConfigSettingEntry
initialContractBandwidthSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_BANDWIDTH_V0);
    auto& e = entry.contractBandwidth();

    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxTxsSizeBytes =
            cfg.TESTING_LEDGER_MAX_TRANSACTIONS_SIZE_BYTES;
        e.txMaxSizeBytes = cfg.TESTING_TX_MAX_SIZE_BYTES;
    }
    else
    {
        e.ledgerMaxTxsSizeBytes =
            InitialSorobanNetworkConfig::LEDGER_MAX_TRANSACTION_SIZES_BYTES;
        e.txMaxSizeBytes = InitialSorobanNetworkConfig::TX_MAX_SIZE_BYTES;
    }

    e.feeTxSize1KB = InitialSorobanNetworkConfig::FEE_TRANSACTION_SIZE_1KB;

    return entry;
}

ConfigSettingEntry
initialContractExecutionLanesSettingsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_EXECUTION_LANES);
    auto& e = entry.contractExecutionLanes();

    if (cfg.USE_CONFIG_FOR_GENESIS)
    {
        e.ledgerMaxTxCount = cfg.TESTING_LEDGER_MAX_SOROBAN_TX_COUNT;
    }
    else
    {
        e.ledgerMaxTxCount = InitialSorobanNetworkConfig::LEDGER_MAX_TX_COUNT;
    }

    return entry;
}

ConfigSettingEntry
initialCpuCostParamsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(
        CONFIG_SETTING_CONTRACT_COST_PARAMS_CPU_INSTRUCTIONS);

    auto& params = entry.contractCostParamsCpuInsns();
    auto const& vals = xdr::xdr_traits<ContractCostType>::enum_values();
    params.resize(static_cast<uint32>(vals.size()));
    for (auto val : vals)
    {
        switch (val)
        {
        case WasmInsnExec:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 6, 0};
            break;

        case WasmMemAlloc:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case HostMemAlloc:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1141, 1};
            break;
        case HostMemCpy:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 39, 24};
            break;
        case HostMemCmp:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 20, 64};
            break;
        case DispatchHostFunction:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 263, 0};
            break;
        case VisitObject:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 108, 0};
            break;
        case ValSer:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 591, 69};
            break;
        case ValDeser:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1112, 34};
            break;
        case ComputeSha256Hash:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 2924, 4149};
            break;
        case ComputeEd25519PubKey:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 25584, 0};
            break;
        case MapEntry:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 53, 0};
            break;
        case VecEntry:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VerifyEd25519Sig:
            params[val] =
                ContractCostParamEntry{ExtensionPoint{0}, 376877, 2747};
            break;
        case VmMemRead:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 182, 24};
            break;
        case VmMemWrite:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 182, 24};
            break;
        case VmInstantiation:
            params[val] =
                ContractCostParamEntry{ExtensionPoint{0}, 967154, 69991};
            break;
        case VmCachedInstantiation:
            params[val] =
                ContractCostParamEntry{ExtensionPoint{0}, 967154, 69991};
            break;
        case InvokeVmFunction:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1125, 0};
            break;
        case ComputeKeccak256Hash:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 2890, 3561};
            break;
        case ComputeEcdsaSecp256k1Key:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 38363, 0};
            break;
        case ComputeEcdsaSecp256k1Sig:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 224, 0};
            break;
        case RecoverEcdsaSecp256k1Key:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1666155, 0};
            break;
        case Int256AddSub:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1716, 0};
            break;
        case Int256Mul:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 2226, 0};
            break;
        case Int256Div:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 2333, 0};
            break;
        case Int256Pow:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 5212, 0};
            break;
        case Int256Shift:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 412, 0};
            break;
        }
    }

    return entry;
}

ConfigSettingEntry
initialStateExpirationSettings(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_STATE_EXPIRATION);

    entry.stateExpirationSettings().autoBumpLedgers =
        InitialSorobanNetworkConfig::AUTO_BUMP_NUM_LEDGERS;
    entry.stateExpirationSettings().maxEntryExpiration =
        InitialSorobanNetworkConfig::MAXIMUM_ENTRY_LIFETIME;

    // TESTING_MINIMUM_PERSISTENT_ENTRY_LIFETIME defaults to
    // InitialSorobanNetworkConfig::MINIMUM_PERSISTENT_ENTRY_LIFETIME
    entry.stateExpirationSettings().minPersistentEntryExpiration =
        cfg.TESTING_MINIMUM_PERSISTENT_ENTRY_LIFETIME;

    entry.stateExpirationSettings().minTempEntryExpiration =
        InitialSorobanNetworkConfig::MINIMUM_TEMP_ENTRY_LIFETIME;
    entry.stateExpirationSettings().bucketListSizeWindowSampleSize =
        InitialSorobanNetworkConfig::BUCKET_LIST_SIZE_WINDOW_SAMPLE_SIZE;

    entry.stateExpirationSettings().bucketListSizeWindowSampleSize =
        InitialSorobanNetworkConfig::BUCKET_LIST_SIZE_WINDOW_SAMPLE_SIZE;
    entry.stateExpirationSettings().evictionScanSize =
        InitialSorobanNetworkConfig::EVICTION_SCAN_SIZE;
    entry.stateExpirationSettings().maxEntriesToExpire =
        InitialSorobanNetworkConfig::MAX_ENTRIES_TO_EXPIRE;

    entry.stateExpirationSettings().persistentRentRateDenominator =
        InitialSorobanNetworkConfig::PERSISTENT_RENT_RATE_DENOMINATOR;
    entry.stateExpirationSettings().tempRentRateDenominator =
        InitialSorobanNetworkConfig::TEMP_RENT_RATE_DENOMINATOR;
    return entry;
}

ConfigSettingEntry
initialMemCostParamsEntry(Config const& cfg)
{
    ConfigSettingEntry entry(CONFIG_SETTING_CONTRACT_COST_PARAMS_MEMORY_BYTES);

    auto& params = entry.contractCostParamsMemBytes();
    auto const& vals = xdr::xdr_traits<ContractCostType>::enum_values();
    params.resize(static_cast<uint32>(vals.size()));
    for (auto val : vals)
    {
        switch (val)
        {
        case WasmInsnExec:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case WasmMemAlloc:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 1, 0};
            break;
        case HostMemAlloc:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 16, 128};
            break;
        case HostMemCpy:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case HostMemCmp:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case DispatchHostFunction:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VisitObject:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case ValSer:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 18, 384};
            break;
        case ValDeser:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 16, 128};
            break;
        case ComputeSha256Hash:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 40, 0};
            break;
        case ComputeEd25519PubKey:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case MapEntry:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VecEntry:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VerifyEd25519Sig:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VmMemRead:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VmMemWrite:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case VmInstantiation:
            params[val] =
                ContractCostParamEntry{ExtensionPoint{0}, 131103, 5080};
            break;
        case VmCachedInstantiation:
            params[val] =
                ContractCostParamEntry{ExtensionPoint{0}, 131103, 5080};
            break;
        case InvokeVmFunction:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 14, 0};
            break;
        case ComputeKeccak256Hash:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 40, 0};
            break;
        case ComputeEcdsaSecp256k1Key:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case ComputeEcdsaSecp256k1Sig:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 0, 0};
            break;
        case RecoverEcdsaSecp256k1Key:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 201, 0};
            break;
        case Int256AddSub:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 119, 0};
            break;
        case Int256Mul:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 119, 0};
            break;
        case Int256Div:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 119, 0};
            break;
        case Int256Pow:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 119, 0};
            break;
        case Int256Shift:
            params[val] = ContractCostParamEntry{ExtensionPoint{0}, 119, 0};
            break;
        }
    }

    return entry;
}

ConfigSettingEntry
initialbucketListSizeWindow(Application& app)
{
    ConfigSettingEntry entry(CONFIG_SETTING_BUCKETLIST_SIZE_WINDOW);

    // Populate 30 day sliding window of BucketList size snapshots with 30
    // copies of the current BL size
    auto blSize = app.getBucketManager().getBucketList().getSize();
    for (auto i = 0;
         i < InitialSorobanNetworkConfig::BUCKET_LIST_SIZE_WINDOW_SAMPLE_SIZE;
         ++i)
    {
        entry.bucketListSizeWindow().push_back(blSize);
    }

    return entry;
}

#endif
}

#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
bool
SorobanNetworkConfig::isValidConfigSettingEntry(ConfigSettingEntry const& cfg)
{
    bool valid = false;
    switch (cfg.configSettingID())
    {
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_MAX_SIZE_BYTES:
        valid = cfg.contractMaxSizeBytes() >=
                MinimumSorobanNetworkConfig::MAX_CONTRACT_SIZE;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_COST_PARAMS_CPU_INSTRUCTIONS:
        valid = SorobanNetworkConfig::isValidCostParams(
            cfg.contractCostParamsCpuInsns());
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_COST_PARAMS_MEMORY_BYTES:
        valid = SorobanNetworkConfig::isValidCostParams(
            cfg.contractCostParamsMemBytes());
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_DATA_KEY_SIZE_BYTES:
        valid = cfg.contractDataKeySizeBytes() >=
                MinimumSorobanNetworkConfig::MAX_CONTRACT_DATA_KEY_SIZE_BYTES;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_DATA_ENTRY_SIZE_BYTES:
        valid = cfg.contractDataEntrySizeBytes() >=
                MinimumSorobanNetworkConfig::MAX_CONTRACT_DATA_ENTRY_SIZE_BYTES;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_EXECUTION_LANES:
        valid = true;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_BANDWIDTH_V0:
        valid = cfg.contractBandwidth().feeTxSize1KB >= 0 &&
                cfg.contractBandwidth().ledgerMaxTxsSizeBytes >=
                    MinimumSorobanNetworkConfig::LEDGER_MAX_TX_SIZE_BYTES &&
                cfg.contractBandwidth().txMaxSizeBytes >=
                    MinimumSorobanNetworkConfig::TX_MAX_SIZE_BYTES;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_COMPUTE_V0:
        valid = cfg.contractCompute().feeRatePerInstructionsIncrement >= 0 &&
                cfg.contractCompute().ledgerMaxInstructions >=
                    MinimumSorobanNetworkConfig::LEDGER_MAX_INSTRUCTIONS &&
                cfg.contractCompute().txMaxInstructions >=
                    MinimumSorobanNetworkConfig::TX_MAX_INSTRUCTIONS &&
                cfg.contractCompute().txMemoryLimit >=
                    MinimumSorobanNetworkConfig::MEMORY_LIMIT;

        valid = valid && cfg.contractCompute().ledgerMaxInstructions >=
                             cfg.contractCompute().txMaxInstructions;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_HISTORICAL_DATA_V0:
        valid = cfg.contractHistoricalData().feeHistorical1KB >= 0;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_LEDGER_COST_V0:
        valid =
            cfg.contractLedgerCost().ledgerMaxReadLedgerEntries >=
                MinimumSorobanNetworkConfig::LEDGER_MAX_READ_LEDGER_ENTRIES &&
            cfg.contractLedgerCost().ledgerMaxReadBytes >=
                MinimumSorobanNetworkConfig::LEDGER_MAX_READ_BYTES &&
            cfg.contractLedgerCost().ledgerMaxWriteLedgerEntries >=
                MinimumSorobanNetworkConfig::LEDGER_MAX_WRITE_LEDGER_ENTRIES &&
            cfg.contractLedgerCost().ledgerMaxWriteBytes >=
                MinimumSorobanNetworkConfig::LEDGER_MAX_WRITE_BYTES &&
            cfg.contractLedgerCost().txMaxReadLedgerEntries >=
                MinimumSorobanNetworkConfig::TX_MAX_READ_LEDGER_ENTRIES &&
            cfg.contractLedgerCost().txMaxReadBytes >=
                MinimumSorobanNetworkConfig::TX_MAX_READ_BYTES &&
            cfg.contractLedgerCost().txMaxWriteLedgerEntries >=
                MinimumSorobanNetworkConfig::TX_MAX_WRITE_LEDGER_ENTRIES &&
            cfg.contractLedgerCost().txMaxWriteBytes >=
                MinimumSorobanNetworkConfig::TX_MAX_WRITE_BYTES &&
            cfg.contractLedgerCost().feeReadLedgerEntry >= 0 &&
            cfg.contractLedgerCost().feeWriteLedgerEntry >= 0 &&
            cfg.contractLedgerCost().feeRead1KB >= 0 &&
            cfg.contractLedgerCost().bucketListTargetSizeBytes > 0 &&
            cfg.contractLedgerCost().writeFee1KBBucketListLow >= 0 &&
            cfg.contractLedgerCost().writeFee1KBBucketListHigh >= 0 &&
            cfg.contractLedgerCost().bucketListWriteFeeGrowthFactor >= 0;

        valid = valid && cfg.contractLedgerCost().ledgerMaxReadLedgerEntries >=
                             cfg.contractLedgerCost().txMaxReadLedgerEntries;
        valid = valid && cfg.contractLedgerCost().ledgerMaxReadBytes >=
                             cfg.contractLedgerCost().txMaxReadBytes;

        valid = valid && cfg.contractLedgerCost().ledgerMaxWriteLedgerEntries >=
                             cfg.contractLedgerCost().txMaxWriteLedgerEntries;
        valid = valid && cfg.contractLedgerCost().ledgerMaxWriteBytes >=
                             cfg.contractLedgerCost().txMaxWriteBytes;
        break;
    case ConfigSettingID::CONFIG_SETTING_CONTRACT_EVENTS_V0:
        valid = cfg.contractEvents().txMaxContractEventsSizeBytes >= 0 &&
                cfg.contractEvents().feeContractEvents1KB >= 0;
        break;
    case ConfigSettingID::CONFIG_SETTING_STATE_EXPIRATION:
        valid =
            cfg.stateExpirationSettings().maxEntryExpiration >=
                MinimumSorobanNetworkConfig::MAXIMUM_ENTRY_LIFETIME &&
            cfg.stateExpirationSettings().minTempEntryExpiration > 0 &&
            cfg.stateExpirationSettings().minPersistentEntryExpiration >=
                MinimumSorobanNetworkConfig::
                    MINIMUM_PERSISTENT_ENTRY_LIFETIME &&
            cfg.stateExpirationSettings().autoBumpLedgers >=
                0 && // autobumpLedgers can be disabled by setting to 0
            cfg.stateExpirationSettings().persistentRentRateDenominator > 0 &&
            cfg.stateExpirationSettings().tempRentRateDenominator > 0 &&
            cfg.stateExpirationSettings().maxEntriesToExpire > 0 &&
            cfg.stateExpirationSettings().bucketListSizeWindowSampleSize > 0 &&
            cfg.stateExpirationSettings().evictionScanSize > 0;

        valid = valid &&
                cfg.stateExpirationSettings().maxEntryExpiration >
                    cfg.stateExpirationSettings().minPersistentEntryExpiration;
        valid =
            valid && cfg.stateExpirationSettings().maxEntryExpiration >
                         cfg.stateExpirationSettings().minTempEntryExpiration;
        break;
    case ConfigSettingID::CONFIG_SETTING_BUCKETLIST_SIZE_WINDOW:
        valid = true;
        break;
    }
    return valid;
}

bool
SorobanNetworkConfig::isNonUpgradeableConfigSettingEntry(
    ConfigSettingEntry const& cfg)
{
    // While the BucketList size window is stored in a ConfigSetting
    // entry, the BucketList defines these values, they should never be
    // changed via upgrade
    return cfg.configSettingID() ==
           ConfigSettingID::CONFIG_SETTING_BUCKETLIST_SIZE_WINDOW;
}

#endif

void
SorobanNetworkConfig::createLedgerEntriesForV20(AbstractLedgerTxn& ltx,
                                                Application& app)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    auto const& cfg = app.getConfig();
    createConfigSettingEntry(initialMaxContractSizeEntry(cfg), ltx);
    createConfigSettingEntry(initialMaxContractDataKeySizeEntry(cfg), ltx);
    createConfigSettingEntry(initialMaxContractDataEntrySizeEntry(cfg), ltx);
    createConfigSettingEntry(initialContractComputeSettingsEntry(cfg), ltx);
    createConfigSettingEntry(initialContractLedgerAccessSettingsEntry(cfg),
                             ltx);
    createConfigSettingEntry(initialContractHistoricalDataSettingsEntry(cfg),
                             ltx);
    createConfigSettingEntry(initialContractEventsSettingsEntry(cfg), ltx);
    createConfigSettingEntry(initialContractBandwidthSettingsEntry(cfg), ltx);
    createConfigSettingEntry(initialContractExecutionLanesSettingsEntry(cfg),
                             ltx);
    createConfigSettingEntry(initialCpuCostParamsEntry(cfg), ltx);
    createConfigSettingEntry(initialMemCostParamsEntry(cfg), ltx);
    createConfigSettingEntry(initialStateExpirationSettings(cfg), ltx);

    createConfigSettingEntry(initialbucketListSizeWindow(app), ltx);
#endif
}

void
SorobanNetworkConfig::initializeGenesisLedgerForTesting(
    uint32_t genesisLedgerProtocol, AbstractLedgerTxn& ltx, Application& app)
{
    if (protocolVersionStartsFrom(genesisLedgerProtocol, ProtocolVersion::V_20))
    {
        SorobanNetworkConfig::createLedgerEntriesForV20(ltx, app);
    }
}

void
SorobanNetworkConfig::loadFromLedger(AbstractLedgerTxn& ltxRoot,
                                     uint32_t configMaxProtocol,
                                     uint32_t protocolVersion)
{
    LedgerTxn ltx(ltxRoot, false, TransactionMode::READ_ONLY_WITHOUT_SQL_TXN);
    loadMaxContractSize(ltx);
    loadMaxContractDataKeySize(ltx);
    loadMaxContractDataEntrySize(ltx);
    loadComputeSettings(ltx);
    loadLedgerAccessSettings(ltx);
    loadHistoricalSettings(ltx);
    loadContractEventsSettings(ltx);
    loadBandwidthSettings(ltx);
    loadCpuCostParams(ltx);
    loadMemCostParams(ltx);
    loadStateExpirationSettings(ltx);
    loadExecutionLanesSettings(ltx);
    loadBucketListSizeWindow(ltx);
    // NB: this should follow loading state expiration settings
    maybeUpdateBucketListWindowSize(ltx);
    // NB: this should follow loading/updating bucket list window
    // size and state expiration settings
    computeWriteFee(configMaxProtocol, protocolVersion);
}

void
SorobanNetworkConfig::loadMaxContractSize(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_MAX_SIZE_BYTES;
    auto le = ltx.loadWithoutRecord(key).current();
    mMaxContractSizeBytes = le.data.configSetting().contractMaxSizeBytes();
#endif
}

void
SorobanNetworkConfig::loadMaxContractDataKeySize(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_DATA_KEY_SIZE_BYTES;
    auto le = ltx.loadWithoutRecord(key).current();
    mMaxContractDataKeySizeBytes =
        le.data.configSetting().contractDataKeySizeBytes();
#endif
}

void
SorobanNetworkConfig::loadMaxContractDataEntrySize(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_DATA_ENTRY_SIZE_BYTES;
    auto le = ltx.loadWithoutRecord(key).current();
    mMaxContractDataEntrySizeBytes =
        le.data.configSetting().contractDataEntrySizeBytes();
#endif
}

void
SorobanNetworkConfig::loadComputeSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_COMPUTE_V0;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting = le.data.configSetting().contractCompute();
    mLedgerMaxInstructions = configSetting.ledgerMaxInstructions;
    mTxMaxInstructions = configSetting.txMaxInstructions;
    mFeeRatePerInstructionsIncrement =
        configSetting.feeRatePerInstructionsIncrement;
    mTxMemoryLimit = configSetting.txMemoryLimit;
#endif
}

void
SorobanNetworkConfig::loadLedgerAccessSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_LEDGER_COST_V0;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting = le.data.configSetting().contractLedgerCost();
    mLedgerMaxReadLedgerEntries = configSetting.ledgerMaxReadLedgerEntries;
    mLedgerMaxReadBytes = configSetting.ledgerMaxReadBytes;
    mLedgerMaxWriteLedgerEntries = configSetting.ledgerMaxWriteLedgerEntries;
    mLedgerMaxWriteBytes = configSetting.ledgerMaxWriteBytes;
    mTxMaxReadLedgerEntries = configSetting.txMaxReadLedgerEntries;
    mTxMaxReadBytes = configSetting.txMaxReadBytes;
    mTxMaxWriteLedgerEntries = configSetting.txMaxWriteLedgerEntries;
    mTxMaxWriteBytes = configSetting.txMaxWriteBytes;
    mFeeReadLedgerEntry = configSetting.feeReadLedgerEntry;
    mFeeWriteLedgerEntry = configSetting.feeWriteLedgerEntry;
    mFeeRead1KB = configSetting.feeRead1KB;
    mBucketListTargetSizeBytes = configSetting.bucketListTargetSizeBytes;
    mWriteFee1KBBucketListLow = configSetting.writeFee1KBBucketListLow;
    mWriteFee1KBBucketListHigh = configSetting.writeFee1KBBucketListHigh;
    mBucketListWriteFeeGrowthFactor =
        configSetting.bucketListWriteFeeGrowthFactor;
#endif
}

void
SorobanNetworkConfig::loadHistoricalSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_HISTORICAL_DATA_V0;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting =
        le.data.configSetting().contractHistoricalData();
    mFeeHistorical1KB = configSetting.feeHistorical1KB;
#endif
}

void
SorobanNetworkConfig::loadContractEventsSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_EVENTS_V0;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting = le.data.configSetting().contractEvents();
    mFeeContractEvents1KB = configSetting.feeContractEvents1KB;
    mTxMaxContractEventsSizeBytes = configSetting.txMaxContractEventsSizeBytes;
#endif
}

void
SorobanNetworkConfig::loadBandwidthSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_BANDWIDTH_V0;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting = le.data.configSetting().contractBandwidth();
    mLedgerMaxTransactionsSizeBytes = configSetting.ledgerMaxTxsSizeBytes;
    mTxMaxSizeBytes = configSetting.txMaxSizeBytes;
    mFeeTransactionSize1KB = configSetting.feeTxSize1KB;
#endif
}

void
SorobanNetworkConfig::loadCpuCostParams(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_COST_PARAMS_CPU_INSTRUCTIONS;
    auto le = ltx.loadWithoutRecord(key).current();
    mCpuCostParams = le.data.configSetting().contractCostParamsCpuInsns();
#endif
}

void
SorobanNetworkConfig::loadMemCostParams(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_COST_PARAMS_MEMORY_BYTES;
    auto le = ltx.loadWithoutRecord(key).current();
    mMemCostParams = le.data.configSetting().contractCostParamsMemBytes();
#endif
}

void
SorobanNetworkConfig::loadExecutionLanesSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_CONTRACT_EXECUTION_LANES;
    auto le = ltx.loadWithoutRecord(key).current();
    auto const& configSetting =
        le.data.configSetting().contractExecutionLanes();
    mLedgerMaxTxCount = configSetting.ledgerMaxTxCount;
#endif
}

void
SorobanNetworkConfig::loadBucketListSizeWindow(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_BUCKETLIST_SIZE_WINDOW;
    auto txle = ltx.loadWithoutRecord(key);
    releaseAssert(txle);
    auto const& leVector =
        txle.current().data.configSetting().bucketListSizeWindow();
    mBucketListSizeSnapshots.clear();
    for (auto e : leVector)
    {
        mBucketListSizeSnapshots.push_back(e);
    }

    updateBucketListSizeAverage();
#endif
}

#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
void
SorobanNetworkConfig::writeBucketListSizeWindow(
    AbstractLedgerTxn& ltxRoot) const
{
    // Check that the window is loaded and the number of snapshots is correct
    releaseAssert(mBucketListSizeSnapshots.size() ==
                  mStateExpirationSettings.bucketListSizeWindowSampleSize);

    // Load outdated snapshot entry from DB
    LedgerTxn ltx(ltxRoot);
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_BUCKETLIST_SIZE_WINDOW;
    auto txle = ltx.load(key);
    releaseAssert(txle);

    // Copy in-memory snapshots to ledger entry
    auto& leVector = txle.current().data.configSetting().bucketListSizeWindow();
    leVector.clear();
    for (auto e : mBucketListSizeSnapshots)
    {
        leVector.push_back(e);
    }

    ltx.commit();
}

void
SorobanNetworkConfig::updateBucketListSizeAverage()
{
    uint64_t sizeSum = 0;
    for (auto const& size : mBucketListSizeSnapshots)
    {
        sizeSum += size;
    }

    mAverageBucketListSize = sizeSum / mBucketListSizeSnapshots.size();
}
#endif

uint32_t
SorobanNetworkConfig::maxContractSizeBytes() const
{
    return mMaxContractSizeBytes;
}

uint32_t
SorobanNetworkConfig::maxContractDataKeySizeBytes() const
{
    return mMaxContractDataKeySizeBytes;
}

uint32_t
SorobanNetworkConfig::maxContractDataEntrySizeBytes() const
{
    return mMaxContractDataEntrySizeBytes;
}

void
SorobanNetworkConfig::loadStateExpirationSettings(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    LedgerKey key(CONFIG_SETTING);
    key.configSetting().configSettingID =
        ConfigSettingID::CONFIG_SETTING_STATE_EXPIRATION;
    auto le = ltx.loadWithoutRecord(key).current();
    mStateExpirationSettings =
        le.data.configSetting().stateExpirationSettings();
#endif
}

// Compute settings for contracts (instructions and memory).
int64_t
SorobanNetworkConfig::ledgerMaxInstructions() const
{
    return mLedgerMaxInstructions;
}

int64_t
SorobanNetworkConfig::txMaxInstructions() const
{
    return mTxMaxInstructions;
}

int64_t
SorobanNetworkConfig::feeRatePerInstructionsIncrement() const
{
    return mFeeRatePerInstructionsIncrement;
}

uint32_t
SorobanNetworkConfig::txMemoryLimit() const
{
    return mTxMemoryLimit;
}

// Ledger access settings for contracts.
uint32_t
SorobanNetworkConfig::ledgerMaxReadLedgerEntries() const
{
    return mLedgerMaxReadLedgerEntries;
}

uint32_t
SorobanNetworkConfig::ledgerMaxReadBytes() const
{
    return mLedgerMaxReadBytes;
}

uint32_t
SorobanNetworkConfig::ledgerMaxWriteLedgerEntries() const
{
    return mLedgerMaxWriteLedgerEntries;
}

uint32_t
SorobanNetworkConfig::ledgerMaxWriteBytes() const
{
    return mLedgerMaxWriteBytes;
}

uint32_t
SorobanNetworkConfig::txMaxReadLedgerEntries() const
{
    return mTxMaxReadLedgerEntries;
}

uint32_t
SorobanNetworkConfig::txMaxReadBytes() const
{
    return mTxMaxReadBytes;
}

uint32_t
SorobanNetworkConfig::txMaxWriteLedgerEntries() const
{
    return mTxMaxWriteLedgerEntries;
}

uint32_t
SorobanNetworkConfig::txMaxWriteBytes() const
{
    return mTxMaxWriteBytes;
}

int64_t
SorobanNetworkConfig::feeReadLedgerEntry() const
{
    return mFeeReadLedgerEntry;
}

int64_t
SorobanNetworkConfig::feeWriteLedgerEntry() const
{
    return mFeeWriteLedgerEntry;
}

int64_t
SorobanNetworkConfig::feeRead1KB() const
{
    return mFeeRead1KB;
}

int64_t
SorobanNetworkConfig::feeWrite1KB() const
{
    return mFeeWrite1KB;
}

// Historical data (pushed to core archives) settings for contracts.
int64_t
SorobanNetworkConfig::feeHistorical1KB() const
{
    return mFeeHistorical1KB;
}

// Maximum size of the emitted contract events.
uint32_t
SorobanNetworkConfig::txMaxContractEventsSizeBytes() const
{
    return mTxMaxContractEventsSizeBytes;
}

int64_t
SorobanNetworkConfig::feeContractEventsSize1KB() const
{
    return mFeeContractEvents1KB;
}

// Bandwidth related data settings for contracts
uint32_t
SorobanNetworkConfig::ledgerMaxTransactionSizesBytes() const
{
    return mLedgerMaxTransactionsSizeBytes;
}

uint32_t
SorobanNetworkConfig::txMaxSizeBytes() const
{
    return mTxMaxSizeBytes;
}

int64_t
SorobanNetworkConfig::feeTransactionSize1KB() const
{
    return mFeeTransactionSize1KB;
}

// General execution lanes settings for contracts
uint32_t
SorobanNetworkConfig::ledgerMaxTxCount() const
{
    return mLedgerMaxTxCount;
}

uint32_t
SorobanNetworkConfig::getBucketListSizeSnapshotPeriod() const
{
#ifdef BUILD_TESTS
    if (mBucketListSnapshotPeriodForTesting)
    {
        return *mBucketListSnapshotPeriodForTesting;
    }
#endif

    return BUCKETLIST_SIZE_SNAPSHOT_PERIOD;
}

void
SorobanNetworkConfig::maybeUpdateBucketListWindowSize(AbstractLedgerTxn& ltx)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    // // Check if BucketList size window should exist
    if (protocolVersionIsBefore(ltx.loadHeader().current().ledgerVersion,
                                ProtocolVersion::V_20))
    {
        return;
    }
    auto currSize = mBucketListSizeSnapshots.size();
    auto newSize = stateExpirationSettings().bucketListSizeWindowSampleSize;
    if (newSize == currSize)
    {
        // No size change, nothing to update
        return;
    }

    if (newSize < currSize)
    {
        while (mBucketListSizeSnapshots.size() != newSize)
        {
            mBucketListSizeSnapshots.pop_front();
        }
    }
    // If newSize > currSize, backfill new slots with oldest value in window
    // such that they are the first to get replaced by new values
    else
    {
        auto oldestSize = mBucketListSizeSnapshots.front();
        while (mBucketListSizeSnapshots.size() != newSize)
        {
            mBucketListSizeSnapshots.push_front(oldestSize);
        }
    }

    updateBucketListSizeAverage();
    writeBucketListSizeWindow(ltx);
#endif
}

void
SorobanNetworkConfig::maybeSnapshotBucketListSize(uint32_t currLedger,
                                                  AbstractLedgerTxn& ltx,
                                                  Application& app)
{
    auto ledgerVersion = ltx.loadHeader().current().ledgerVersion;
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    // // Check if BucketList size window should exist
    if (protocolVersionIsBefore(ledgerVersion, ProtocolVersion::V_20))
    {
        return;
    }

    if (currLedger % getBucketListSizeSnapshotPeriod() == 0)
    {
        // Update in memory snapshots
        mBucketListSizeSnapshots.pop_front();
        mBucketListSizeSnapshots.push_back(
            app.getBucketManager().getBucketList().getSize());

        writeBucketListSizeWindow(ltx);
        updateBucketListSizeAverage();
        computeWriteFee(app.getConfig().CURRENT_LEDGER_PROTOCOL_VERSION,
                        ledgerVersion);
    }
#endif
}

uint64_t
SorobanNetworkConfig::getAverageBucketListSize() const
{
    return mAverageBucketListSize;
}

#ifdef BUILD_TESTS
uint32_t&
SorobanNetworkConfig::maxContractDataKeySizeBytes()
{
    return mMaxContractDataKeySizeBytes;
}

uint32_t&
SorobanNetworkConfig::maxContractDataEntrySizeBytes()
{
    return mMaxContractDataEntrySizeBytes;
}

void
SorobanNetworkConfig::setBucketListSnapshotPeriodForTesting(uint32_t period)
{
    mBucketListSnapshotPeriodForTesting = period;
}

std::deque<uint64_t> const&
SorobanNetworkConfig::getBucketListSizeWindowForTesting() const
{
    return mBucketListSizeSnapshots;
}
#endif

#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION

ContractCostParams const&
SorobanNetworkConfig::cpuCostParams() const
{
    return mCpuCostParams;
}

ContractCostParams const&
SorobanNetworkConfig::memCostParams() const
{
    return mMemCostParams;
}

StateExpirationSettings const&
SorobanNetworkConfig::stateExpirationSettings() const
{
    return mStateExpirationSettings;
}

#ifdef BUILD_TESTS
StateExpirationSettings&
SorobanNetworkConfig::stateExpirationSettings()
{
    return mStateExpirationSettings;
}
#endif

bool
SorobanNetworkConfig::isValidCostParams(ContractCostParams const& params)
{
    if (params.size() !=
        xdr::xdr_traits<ContractCostType>::enum_values().size())
    {
        return false;
    }

    for (auto const& param : params)
    {
        if (param.constTerm < 0 || param.linearTerm < 0)
        {
            return false;
        }
    }

    return true;
}

CxxFeeConfiguration
SorobanNetworkConfig::rustBridgeFeeConfiguration() const
{
    CxxFeeConfiguration res;
    res.fee_per_instruction_increment = feeRatePerInstructionsIncrement();

    res.fee_per_read_entry = feeReadLedgerEntry();
    res.fee_per_write_entry = feeWriteLedgerEntry();
    res.fee_per_read_1kb = feeRead1KB();
    // This should be dependent on the ledger size, but initially
    // we'll just use the flat rate here.
    res.fee_per_write_1kb = feeWrite1KB();

    res.fee_per_transaction_size_1kb = feeTransactionSize1KB();

    res.fee_per_contract_event_1kb = feeContractEventsSize1KB();

    res.fee_per_historical_1kb = feeHistorical1KB();

    return res;
}

CxxRentFeeConfiguration
SorobanNetworkConfig::rustBridgeRentFeeConfiguration() const
{
    CxxRentFeeConfiguration res{};
    auto const& cfg = stateExpirationSettings();
    res.fee_per_write_1kb = feeWrite1KB();
    res.persistent_rent_rate_denominator = cfg.persistentRentRateDenominator;
    res.temporary_rent_rate_denominator = cfg.tempRentRateDenominator;
    return res;
}
#endif

void
SorobanNetworkConfig::computeWriteFee(uint32_t configMaxProtocol,
                                      uint32_t protocolVersion)
{
#ifdef ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
    CxxWriteFeeConfiguration feeConfig{};
    feeConfig.bucket_list_target_size_bytes = mBucketListTargetSizeBytes;
    feeConfig.bucket_list_write_fee_growth_factor =
        mBucketListWriteFeeGrowthFactor;
    feeConfig.write_fee_1kb_bucket_list_low = mWriteFee1KBBucketListLow;
    feeConfig.write_fee_1kb_bucket_list_high = mWriteFee1KBBucketListHigh;
    // This may throw, but only if core is mis-configured.
    mFeeWrite1KB = rust_bridge::compute_write_fee_per_1kb(
        configMaxProtocol, protocolVersion, mAverageBucketListSize, feeConfig);
#endif
}
} // namespace stellar
