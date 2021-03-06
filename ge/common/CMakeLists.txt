set(PROTO_LIST
    "${METADEF_DIR}/proto/om.proto"
    "${METADEF_DIR}/proto/ge_ir.proto"
    "${METADEF_DIR}/proto/insert_op.proto"
    "${METADEF_DIR}/proto/task.proto"
    "${METADEF_DIR}/proto/tensorflow/attr_value.proto"
    "${METADEF_DIR}/proto/tensorflow/function.proto"
    "${METADEF_DIR}/proto/tensorflow/graph.proto"
    "${METADEF_DIR}/proto/tensorflow/node_def.proto"
    "${METADEF_DIR}/proto/tensorflow/op_def.proto"
    "${METADEF_DIR}/proto/tensorflow/resource_handle.proto"
    "${METADEF_DIR}/proto/tensorflow/tensor.proto"
    "${METADEF_DIR}/proto/tensorflow/tensor_shape.proto"
    "${METADEF_DIR}/proto/tensorflow/types.proto"
    "${METADEF_DIR}/proto/tensorflow/versions.proto" 
)

protobuf_generate(ge PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

set(SRC_LIST
    "context/ctx.cc"
    "model_saver.cc"
    "ge/datatype_util.cc"
    "helper/om_file_helper.cc"
    "helper/model_helper.cc"
    "../model/ge_model.cc"
    "auth/file_saver.cc"
    "fp16_t.cc"
    "math/fp16_math.cc"
    "debug/memory_dumper.cc"
    "formats/utils/formats_trans_utils.cc"
    "dump/dump_properties.cc"
    "formats/format_transfers/datatype_transfer.cc"
    "formats/format_transfers/format_transfer_transpose.cc"
    "formats/format_transfers/format_transfer_nchw_nc1hwc0.cc"
    "formats/format_transfers/format_transfer_fractal_z.cc"
    "formats/format_transfers/format_transfer_fractal_nz.cc"
    "formats/format_transfers/format_transfer_fractal_zz.cc"
    "formats/format_transfers/format_transfer_nhwc_nc1hwc0.cc"
    "formats/format_transfers/format_transfer_nc1hwc0_nchw.cc"
    "formats/format_transfers/format_transfer_nc1hwc0_nhwc.cc"
    "formats/format_transfers/format_transfer_hwcn_c1hwncoc0.cc"
    "formats/format_transfers/format_transfer_c1hwncoc0_hwcn.cc"
    "formats/format_transfers/format_transfer_fracz_nchw.cc"
    "formats/format_transfers/format_transfer_fracz_nhwc.cc"
    "formats/format_transfers/format_transfer_fracz_hwcn.cc"
    "formats/format_transfers/format_transfer_dhwcn_fracz3D.cc"
    "formats/format_transfers/format_transfer_dhwnc_fracz3D_transpose.cc"
    "formats/format_transfers/format_transfer_nchw_fz_c04.cc"
    "formats/formats.cc"
    "ge_format_util.cc"
    "fmk_error_codes.cc"
    "util.cc"
    "properties_manager.cc"
    "types.cc"
    "model_parser/base.cc"
    "kernel_store.cc"
    "tbe_kernel_store.cc"
    "cust_aicpu_kernel_store.cc"
    "op/attr_value_util.cc"
    "op/ge_op_utils.cc"
    "thread_pool.cc"
    "ge/tbe_plugin_manager.cc"
)

############ libge_common.so ############
add_library(ge_common SHARED ${SRC_LIST} ${PROTO_HDRS})
target_compile_definitions(ge_common PRIVATE
    PROTOBUF_INLINE_NOT_IN_HEADERS=0
    HOST_VISIBILITY
    FMK_SUPPORT_DUMP
    OS_CENTOS
)

target_compile_options(ge_common PRIVATE
    -fvisibility=hidden
    -O2
    -Werror
)

target_include_directories(ge_common PRIVATE
    ${GE_CODE_DIR}/ge
    ${GE_CODE_DIR}/ge/common
    ${GE_CODE_DIR}/ge/common/op
    ${GE_CODE_DIR}/inc/external
    ${GE_CODE_DIR}/inc
    ${GE_CODE_DIR}/inc/framework
    ${METADEF_DIR}/inc
    ${METADEF_DIR}/inc/external
    ${METADEF_DIR}/inc/external/graph
    ${METADEF_DIR}/inc/graph
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/proto/ge
    #### yellow zone ####
    ${GE_DEPEND_DIR}/inc
    ${GE_DEPEND_DIR}/inc/cce
    #### blue zone ####
    #${GE_DEPEND_DIR}/include
    ${GE_CODE_DIR}/third_party/fwkacllib/inc
    ${GE_CODE_DIR}/third_party/fwkacllib/inc/toolchain
)

target_link_libraries(ge_common PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    -Wl,--no-as-needed
    graph
    protobuf
    register
    c_sec
    error_manager
    slog
    mmpa
    -Wl,--as-needed
    json
    -lrt
    -ldl
)

############ libge_common.a ############
add_library(ge_common_static STATIC ${SRC_LIST} ${PROTO_HDRS})
target_compile_definitions(ge_common_static PRIVATE
    PROTOBUF_INLINE_NOT_IN_HEADERS=0
    HOST_VISIBILITY
    FMK_SUPPORT_DUMP
    OS_CENTOS
)

target_compile_options(ge_common_static PRIVATE
    -fvisibility=hidden
    -O2
    -Werror
)

target_include_directories(ge_common_static PRIVATE
    ${GE_CODE_DIR}/ge
    ${GE_CODE_DIR}/ge/common
    ${GE_CODE_DIR}/ge/common/op
    ${GE_CODE_DIR}/inc
    ${GE_CODE_DIR}/inc/external
    ${GE_CODE_DIR}/inc/framework
    ${METADEF_DIR}/inc
    ${METADEF_DIR}/inc/external
    ${METADEF_DIR}/inc/external/graph
    ${METADEF_DIR}/inc/graph
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/proto/ge
    #### yellow zone ####
    ${GE_DEPEND_DIR}/inc
    ${GE_DEPEND_DIR}/inc/cce
    #### blue zone ####
    #${GE_DEPEND_DIR}/include
    ${GE_CODE_DIR}/third_party/fwkacllib/inc
    ${GE_CODE_DIR}/third_party/fwkacllib/inc/toolchain
)

target_link_libraries(ge_common_static PRIVATE
    $<BUILD_INTERFACE:intf_pub>
    protobuf
    json
    c_sec
    -lrt
    -ldl
)

############ install ############
set(INSTALL_BASE_DIR "")
set(INSTALL_LIBRARY_DIR lib)

install(TARGETS ge_common OPTIONAL
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR}
)
