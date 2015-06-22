// Copyright (c) 2011-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "txmempool.h"
#include "util.h"
#include "uint256.h"

#include "test/test_bitcoin.h"

#include <boost/test/unit_test.hpp>
#include <list>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolReplaceTest)
{
    // Test AcceptToMemoryPool replacing functionality
    CTxMemPool testPool(CFeeRate(0));
    CValidationState state;
    bool missingInputs;
    bool accepted = false;

    // Output to conflict
    CMutableTransaction txConflict;
    txConflict.vin.resize(1);
    txConflict.vin[0].scriptSig = CScript();
    txConflict.vin[0].prevout.hash = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
    txConflict.vin[0].prevout.n = 0;
    txConflict.vout.resize(1);
    txConflict.vout[0].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("88d9931ea73d60eaf7e5671efc0552b912911f2a") << OP_EQUALVERIFY << OP_CHECKSIG;
    txConflict.vout[0].nValue = 1000000LL;
    testPool.addUnchecked(txConflict.GetHash(), CTxMemPoolEntry(txConflict, 0, 0, 0.0, 1));

    // Should replace a conflicting tx if all the outputs are unspendable

    // Setup a transaction that we can replace later
    CMutableTransaction txUnspendable;
    txUnspendable.vin.resize(1);
    txUnspendable.vin[0].scriptSig = CScript() << ParseHex("3044022008f5bc8238540865d82841ef9462413b3b9d5bd6ebc1e731477fd22a64e26fc40220350a6b8f4442e8e4cf0cf47c2db888a01e3dcb96557aa71a7dc076386c31cca501") << ParseHex("0223078d2942df62c45621d209fab84ea9a7a23346201b7727b9b45a29c4e76f5e");
    txUnspendable.vin[0].prevout.hash = txConflict.GetHash();
    txUnspendable.vin[0].prevout.n = 0;
    txUnspendable.vout.resize(1);
    txUnspendable.vout[0].scriptPubKey = CScript() << OP_RETURN;
    txUnspendable.vout[0].nValue = 900000LL;

    // And add it to the mempool
    accepted = false;
    accepted = AcceptToMemoryPool(testPool, state, txUnspendable, false, &missingInputs, true);
    BOOST_CHECK_EQUAL(accepted, true);
    testPool.addUnchecked(txUnspendable.GetHash(), CTxMemPoolEntry(txUnspendable, 0, 0, 0.0, 1));

    // Then, modify the transaction with higher fees and only unspendable outputs,
    // and it should replace in the mempool
    txUnspendable.vin[0].scriptSig = CScript() << ParseHex("3045022100b5c012e01a1d64f85c7a6b34891b0725ecc58323b8cf0c6ebae8c62d3bbf482f022023d324a2c620d6ee41e6485499f509e066be4df6544f92f3971ea5b56a1a432f01") << ParseHex("0223078d2942df62c45621d209fab84ea9a7a23346201b7727b9b45a29c4e76f5e");
    txUnspendable.vout[0].nValue = 800000LL;
    accepted = false;
    accepted = AcceptToMemoryPool(testPool, state, txUnspendable, false, &missingInputs, true);
    BOOST_CHECK_EQUAL(accepted, true);

    // Lastly, it should replace again with a tx with unspendable and change outputs
    txUnspendable.vin[0].scriptSig = CScript() << ParseHex("3045022100f0455806c14e6d2a1fcb6b1d10828c0cb825521045fe690470b27565686d48d0022018a625cc550bcef509befb1ececb73e9c46c364e06d2a20efcf4f5ba6cdfaf9401") << ParseHex("0223078d2942df62c45621d209fab84ea9a7a23346201b7727b9b45a29c4e76f5e");
    txUnspendable.vout.resize(2);
    txUnspendable.vout[0].scriptPubKey = CScript() << OP_RETURN;
    txUnspendable.vout[0].nValue = 0LL;
    txUnspendable.vout[1].scriptPubKey = CScript() << OP_DUP << OP_HASH160 << ParseHex("88d9931ea73d60eaf7e5671efc0552b912911f2a") << OP_EQUALVERIFY << OP_CHECKSIG;
    txUnspendable.vout[1].nValue = 700LL;
    accepted = false;
    accepted = AcceptToMemoryPool(testPool, state, txUnspendable, false, &missingInputs, true);
    BOOST_CHECK_EQUAL(accepted, true);
}

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
    // Test CTxMemPool::remove functionality

    // Parent transaction with three children,
    // and three grand-children:
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = 33000LL;
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout.hash = txParent.GetHash();
        txChild[i].vin[0].prevout.n = i;
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = 11000LL;
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout.hash = txChild[i].GetHash();
        txGrandChild[i].vin[0].prevout.n = 0;
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = 11000LL;
    }


    CTxMemPool testPool(CFeeRate(0));
    std::list<CTransaction> removed;

    // Nothing in pool, remove should do nothing:
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);

    // Just the parent:
    testPool.addUnchecked(txParent.GetHash(), CTxMemPoolEntry(txParent, 0, 0, 0.0, 1));
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 1);
    removed.clear();
    
    // Parent, children, grandchildren:
    testPool.addUnchecked(txParent.GetHash(), CTxMemPoolEntry(txParent, 0, 0, 0.0, 1));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), CTxMemPoolEntry(txChild[i], 0, 0, 0.0, 1));
        testPool.addUnchecked(txGrandChild[i].GetHash(), CTxMemPoolEntry(txGrandChild[i], 0, 0, 0.0, 1));
    }
    // Remove Child[0], GrandChild[0] should be removed:
    testPool.remove(txChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 2);
    removed.clear();
    // ... make sure grandchild and child are gone:
    testPool.remove(txGrandChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);
    testPool.remove(txChild[0], removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 0);
    // Remove parent, all children/grandchildren should go:
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 5);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
    removed.clear();

    // Add children and grandchildren, but NOT the parent (simulate the parent being in a block)
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), CTxMemPoolEntry(txChild[i], 0, 0, 0.0, 1));
        testPool.addUnchecked(txGrandChild[i].GetHash(), CTxMemPoolEntry(txGrandChild[i], 0, 0, 0.0, 1));
    }
    // Now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    testPool.remove(txParent, removed, true);
    BOOST_CHECK_EQUAL(removed.size(), 6);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
    removed.clear();
}

BOOST_AUTO_TEST_SUITE_END()
