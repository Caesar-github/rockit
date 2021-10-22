/*
 * Copyright 2021 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include "gtest/gtest.h"

#include "rk_mpi_sys.h"

GTEST_API_ int main(int argc, const char **argv) {
    testing::InitGoogleTest(&argc, const_cast<char **>(argv));
    RK_S32 s32Ret = RK_SUCCESS;

    s32Ret = RK_MPI_SYS_Init();
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

    s32Ret = RUN_ALL_TESTS();
    if (s32Ret != RK_SUCCESS) {
        goto __FAILED;
    }

__FAILED:
    RK_MPI_SYS_Exit();

    return s32Ret;
}
