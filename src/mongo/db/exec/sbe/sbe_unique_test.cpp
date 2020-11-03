/**
 *    Copyright (C) 2018-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */

#include "mongo/platform/basic.h"

#include <string_view>

#include "mongo/db/exec/sbe/sbe_plan_stage_test.h"
#include "mongo/db/exec/sbe/stages/unique.h"

namespace mongo::sbe {
/**
 * This file contains tests for sbe::UniqueStage.
 */

using UniqueStageTest = PlanStageTestFixture;

TEST_F(UniqueStageTest, DeduplicatesAndPreservesOrderSimple) {
    auto [inputTag, inputVal] = makeValue(BSON_ARRAY(1 << 2 << 3 << 1));
    value::ValueGuard inputGuard{inputTag, inputVal};

    auto [expectedTag, expectedVal] = makeValue(BSON_ARRAY(1 << 2 << 3));
    value::ValueGuard expectedGuard{expectedTag, expectedVal};

    auto makeStageFn = [](value::SlotId scanSlot, std::unique_ptr<PlanStage> scanStage) {
        auto unique =
            makeS<UniqueStage>(std::move(scanStage), sbe::makeSV(scanSlot), kEmptyPlanNodeId);

        return std::make_pair(scanSlot, std::move(unique));
    };

    inputGuard.reset();
    expectedGuard.reset();
    runTest(inputTag, inputVal, expectedTag, expectedVal, makeStageFn);
}

TEST_F(UniqueStageTest, DeduplicatesMultipleSlotsInKey) {
    auto [tag, val] = makeValue(BSON_ARRAY(
        BSON_ARRAY(1 << 1) << BSON_ARRAY(2 << 2) << BSON_ARRAY(1 << 1) << BSON_ARRAY(3 << 3)));
    auto [scanSlots, scan] = generateMockScanMulti(2,  // numSlots
                                                   tag,
                                                   val);

    auto [expectedTag, expectedVal] =
        makeValue(BSON_ARRAY(BSON_ARRAY(1 << 1) << BSON_ARRAY(2 << 2) << BSON_ARRAY(3 << 3)));
    value::ValueGuard expectedGuard{expectedTag, expectedVal};

    auto unique = makeS<UniqueStage>(std::move(scan), scanSlots, kEmptyPlanNodeId);

    auto resultAccessors = prepareTree(unique.get(), scanSlots);

    auto [resultsTag, resultsVal] = getAllResultsMulti(unique.get(), resultAccessors);
    value::ValueGuard resultGuard{resultsTag, resultsVal};

    ASSERT_TRUE(valueEquals(resultsTag, resultsVal, expectedTag, expectedVal));
}
}  // namespace mongo::sbe