package com._4paradigm.openmldb.test_common.chain.result;

import com._4paradigm.openmldb.test_common.bean.OpenMLDBResult;

public class ResultParserManager {
    private AbstractResultHandler resultHandler;
    private ResultParserManager() {
        DescResultParser selectResultHandler = new DescResultParser();

        resultHandler = selectResultHandler;
    }

    private static class ClassHolder {
        private static final ResultParserManager holder = new ResultParserManager();
    }

    public static ResultParserManager of() {
        return ClassHolder.holder;
    }
    public void parseResult(OpenMLDBResult openMLDBResult){
        resultHandler.doHandle(openMLDBResult);
    }
    
}
