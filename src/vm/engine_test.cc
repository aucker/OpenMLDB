/*
 * engine_test.cc
 * Copyright (C) 4paradigm.com 2019 wangtaize <wangtaize@4paradigm.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vm/engine.h"
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/AggressiveInstCombine/AggressiveInstCombine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "parser/parser.h"
#include "plan/planner.h"
#include "storage/codec.h"
#include "storage/window.h"
#include "vm/table_mgr.h"

using namespace llvm;       // NOLINT (build/namespaces)
using namespace llvm::orc;  // NOLINT (build/namespaces)

namespace fesql {
namespace vm {

class TableMgrImpl : public TableMgr {
 public:
    explicit TableMgrImpl(std::shared_ptr<TableStatus> status)
        : status_(status) {}
    ~TableMgrImpl() {}
    std::shared_ptr<TableStatus> GetTableDef(const std::string&,
                                             const std::string&) {
        return status_;
    }
    std::shared_ptr<TableStatus> GetTableDef(const std::string&,
                                             const uint32_t) {
        return status_;
    }

 private:
    std::shared_ptr<TableStatus> status_;
};

class EngineTest : public ::testing::Test {};

void BuildBuf(int8_t** buf, uint32_t* size) {
    ::fesql::type::TableDef table;
    table.set_name("t1");
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col5");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col6");
    }

    storage::RowBuilder builder(table.columns());
    uint32_t total_size = builder.CalTotalLength(1);
    int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
    builder.SetBuffer(ptr, total_size);
    builder.AppendInt32(32);
    builder.AppendInt16(16);
    builder.AppendFloat(2.1f);
    builder.AppendDouble(3.1);
    builder.AppendInt64(64);
    builder.AppendString("1", 1);
    *buf = ptr;
    *size = total_size;
}

void BuildWindow(int8_t** buf) {
    ::fesql::type::TableDef table;
    table.set_name("t1");
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }
    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col5");
    }

    {
        ::fesql::type::ColumnDef* column = table.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col6");
    }

    std::vector<fesql::storage::Row> rows;

    {
        storage::RowBuilder builder(table.columns());
        std::string str = "1";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));

        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(1);
        builder.AppendInt16(5);
        builder.AppendFloat(1.1f);
        builder.AppendDouble(11.1);
        builder.AppendInt64(1);
        builder.AppendString(str.c_str(), 1);
        rows.push_back(fesql::storage::Row{.buf = ptr, .size = total_size});
    }
    {
        storage::RowBuilder builder(table.columns());
        std::string str = "22";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(2);
        builder.AppendInt16(5);
        builder.AppendFloat(2.2f);
        builder.AppendDouble(22.2);
        builder.AppendInt64(2);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(fesql::storage::Row{.buf = ptr, .size = total_size});
    }
    {
        storage::RowBuilder builder(table.columns());
        std::string str = "333";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(3);
        builder.AppendInt16(55);
        builder.AppendFloat(3.3f);
        builder.AppendDouble(33.3);
        builder.AppendInt64(1);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(fesql::storage::Row{.buf = ptr, .size = total_size});
    }
    {
        storage::RowBuilder builder(table.columns());
        std::string str = "4444";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(4);
        builder.AppendInt16(55);
        builder.AppendFloat(4.4f);
        builder.AppendDouble(44.4);
        builder.AppendInt64(2);
        builder.AppendString("4444", str.size());
        rows.push_back(fesql::storage::Row{.buf = ptr, .size = total_size});
    }
    {
        storage::RowBuilder builder(table.columns());
        std::string str =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
            "a";
        uint32_t total_size = builder.CalTotalLength(str.size());
        int8_t* ptr = static_cast<int8_t*>(malloc(total_size));
        builder.SetBuffer(ptr, total_size);
        builder.AppendInt32(5);
        builder.AppendInt16(55);
        builder.AppendFloat(5.5f);
        builder.AppendDouble(55.5);
        builder.AppendInt64(3);
        builder.AppendString(str.c_str(), str.size());
        rows.push_back(fesql::storage::Row{.buf = ptr, .size = total_size});
    }

    ::fesql::storage::WindowIteratorImpl* w =
        new ::fesql::storage::WindowIteratorImpl(rows);
    *buf = reinterpret_cast<int8_t*>(w);
}

TEST_F(EngineTest, test_normal) {
    std::unique_ptr<::fesql::storage::Table> table(
        new ::fesql::storage::Table("t1", 1, 1, 1));
    ASSERT_TRUE(table->Init());
    int8_t* row1 = NULL;
    uint32_t size1 = 0;
    BuildBuf(&row1, &size1);

    ASSERT_TRUE(table->Put("k1", 1, reinterpret_cast<char*>(row1), size1));
    ASSERT_TRUE(table->Put("k1", 2, reinterpret_cast<char*>(row1), size1));
    std::shared_ptr<TableStatus> status(new TableStatus());
    status->table = std::move(table);
    status->table_def.set_name("t1");
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }

    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }

    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col15");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col6");
    }

    TableMgrImpl table_mgr(status);
    const std::string sql =
        "%%fun\ndef test(a:i32,b:i32):i32\n    c=a+b\n    d=c+1\n    return "
        "d\nend\n%%sql\nSELECT test(col1,col1), col2 , col6 FROM t1 limit 10;";
    Engine engine(&table_mgr);
    RunSession session;
    base::Status get_status;
    bool ok = engine.Get(sql, "db", session, get_status);
    ASSERT_TRUE(ok);
    std::vector<int8_t*> output;
    int32_t ret = session.Run(output, 2);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(2, output.size());
    int8_t* output1 = output[0];
    int8_t* output2 = output[1];
    free(output1);
    free(output2);
}

TEST_F(EngineTest, test_window_agg) {
    std::unique_ptr<::fesql::storage::Table> table(
        new ::fesql::storage::Table("t1", 1, 1, 1));
    ASSERT_TRUE(table->Init());
    int8_t* rows = NULL;
    BuildWindow(&rows);

    ::fesql::storage::WindowIteratorImpl* w =
        reinterpret_cast<::fesql::storage::WindowIteratorImpl*>(rows);
    ASSERT_TRUE(w->Valid());
    ::fesql::storage::Row row = w->Next();
    ASSERT_TRUE(table->Put("5", 1, reinterpret_cast<char*>(row.buf), row.size));

    ASSERT_TRUE(w->Valid());
    row = w->Next();
    ASSERT_TRUE(table->Put("5", 2, reinterpret_cast<char*>(row.buf), row.size));

    ASSERT_TRUE(w->Valid());
    row = w->Next();
    ASSERT_TRUE(
        table->Put("55", 1, reinterpret_cast<char*>(row.buf), row.size));
    ASSERT_TRUE(w->Valid());
    row = w->Next();
    ASSERT_TRUE(
        table->Put("55", 2, reinterpret_cast<char*>(row.buf), row.size));
    ASSERT_TRUE(w->Valid());
    row = w->Next();
    ASSERT_TRUE(
        table->Put("55", 3, reinterpret_cast<char*>(row.buf), row.size));

    std::shared_ptr<TableStatus> status(new TableStatus());
    status->table = std::move(table);
    status->table_def.set_name("t1");
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt32);
        column->set_name("col1");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt16);
        column->set_name("col2");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kFloat);
        column->set_name("col3");
    }

    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kDouble);
        column->set_name("col4");
    }

    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kInt64);
        column->set_name("col5");
    }
    {
        ::fesql::type::ColumnDef* column = status->table_def.add_columns();
        column->set_type(::fesql::type::kVarchar);
        column->set_name("col6");
    }

    TableMgrImpl table_mgr(status);
    const std::string sql =
        "SELECT "
        "sum(col1) OVER w1 as w1_col1_sum, "
        "sum(col3) OVER w1 as w1_col3_sum, "
        "sum(col4) OVER w1 as w1_col4_sum, "
        "sum(col2) OVER w1 as w1_col2_sum, "
        "sum(col5) OVER w1 as w1_col5_sum "
        "FROM t1 WINDOW w1 AS (PARTITION BY col2 ORDER BY col5 ROWS BETWEEN 3 "
        "PRECEDING AND CURRENT ROW) limit 10;";
    Engine engine(&table_mgr);
    RunSession session;
    base::Status get_status;
    bool ok = engine.Get(sql, "db", session, get_status);
    ASSERT_TRUE(ok);
    const uint32_t length = session.GetRowSize();
    std::vector<int8_t*> output;
    int32_t ret = session.Run(output, 10);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(5, output.size());
    ASSERT_EQ(length, 28);

    //    ASSERT_EQ(15, *((int32_t*)(output[0]+2)));
    //    ASSERT_EQ(1, *((int32_t*)(output[0] + 7)));
    //    ASSERT_EQ(1, *((int32_t*)(output[0] +11)));

    ASSERT_EQ(7 + 4 + 4 + 8 + 2 + 8, *((int32_t*)(output[1] + 2)));
    ASSERT_EQ(1 + 2, *((int32_t*)(output[1] + 7)));
    ASSERT_EQ(1.1f + 2.2f, *((float*)(output[1] + 7 + 4)));
    ASSERT_EQ(11.1 + 22.2, *((double*)(output[1] + 7 + 4 + 4)));
    ASSERT_EQ(5u + 5u, *((int16_t*)(output[1] + 7 + 4 + 4 + 8)));
    ASSERT_EQ(1L + 2L, *((int64_t*)(output[1] + 7 + 4 + 4 + 8 + 2)));

    ASSERT_EQ(7 + 4 + 4 + 8 + 2 + 8,
              *(reinterpret_cast<int32_t*>(output[2] + 2)));
    ASSERT_EQ(3 + 4 + 5, *((int32_t*)(output[4] + 7)));
    ASSERT_EQ(3.3f + 4.4f + 5.5f, *((float*)(output[4] + 7 + 4)));
    ASSERT_EQ(33.3 + 44.4 + 55.5, *((double*)(output[4] + 7 + 4 + 4)));
    ASSERT_EQ(55u + 55u + 55u, *((int16_t*)(output[4] + 7 + 4 + 4 + 8)));
    ASSERT_EQ(1L + 2L + 3L, *((int64_t*)(output[4] + 7 + 4 + 4 + 8 + 2)));
    //    ASSERT_EQ(3+4, *((int32_t*)(output[3] + 2)));
    //    ASSERT_EQ(3+4+5, *((int32_t*)(output[4] + 2)));
    //    ASSERT_EQ(4+5+6, *((int32_t*)(output[5] + 2)));
    for (auto ptr : output) {
        free(ptr);
    }
}

}  // namespace vm
}  // namespace fesql

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    InitializeNativeTarget();
    InitializeNativeTargetAsmPrinter();
    return RUN_ALL_TESTS();
}
