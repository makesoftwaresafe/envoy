load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_fuzz_test",
    "envoy_cc_test",
    "envoy_contrib_package",
    "envoy_proto_library",
)

licenses(["notice"])  # Apache 2

envoy_contrib_package()

envoy_cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    data = [
        "//contrib/golang/filters/http/test/test_data:plugins.so",
    ],
    rbe_pool = "6gig",
    deps = [
        "//contrib/golang/filters/http/source:config",
        "//contrib/golang/filters/http/test/test_data/destroyconfig:destroyconfig_test_lib",
        "//test/mocks/server:factory_context_mocks",
        "//test/test_common:utility_lib",
    ],
)

envoy_cc_test(
    name = "golang_filter_test",
    srcs = ["golang_filter_test.cc"],
    data = [
        "//contrib/golang/filters/http/test/test_data:plugins.so",
    ],
    rbe_pool = "6gig",
    deps = [
        "//contrib/golang/filters/http/source:golang_filter_lib",
        "//source/common/stream_info:stream_info_lib",
        "//test/mocks/api:api_mocks",
        "//test/mocks/http:http_mocks",
        "//test/mocks/network:network_mocks",
        "//test/mocks/server:factory_context_mocks",
        "//test/mocks/ssl:ssl_mocks",
        "//test/mocks/thread_local:thread_local_mocks",
        "//test/mocks/upstream:cluster_manager_mocks",
        "//test/test_common:logging_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/config/core/v3:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "golang_integration_test",
    srcs = ["golang_integration_test.cc"],
    data = [
        "//contrib/golang/filters/http/test/test_data:plugins.so",
    ],
    rbe_pool = "6gig",
    deps = [
        "//contrib/golang/filters/http/source:config",
        "//source/exe:main_common_lib",
        "//source/extensions/filters/http/lua:config",
        "//test/config:v2_link_hacks",
        "//test/integration:http_integration_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/filters/network/http_connection_manager/v3:pkg_cc_proto",
    ],
)

envoy_proto_library(
    name = "golang_filter_fuzz_proto",
    srcs = ["golang_filter_fuzz.proto"],
    deps = [
        "//test/fuzz:common_proto",
        "@envoy_api//contrib/envoy/extensions/filters/http/golang/v3alpha:pkg",
        "@envoy_api//envoy/config/core/v3:pkg",
    ],
)

envoy_cc_fuzz_test(
    name = "golang_filter_fuzz_test",
    srcs = ["golang_filter_fuzz_test.cc"],
    corpus = "golang_filter_corpus",
    rbe_pool = "6gig",
    deps = [
        ":golang_filter_fuzz_proto_cc_proto",
        "//contrib/golang/common/dso/test:dso_mocks",
        "//contrib/golang/filters/http/source:config",
        "//source/common/network:address_lib",
        "//test/extensions/filters/http/common/fuzz:http_filter_fuzzer_lib",
        "//test/mocks/http:http_mocks",
        "//test/mocks/network:network_mocks",
        "//test/mocks/runtime:runtime_mocks",
        "//test/mocks/server:factory_context_mocks",
        "@envoy_api//contrib/envoy/extensions/filters/http/golang/v3alpha:pkg_cc_proto",
    ],
)

envoy_cc_test(
    name = "websocket_integration_test",
    size = "large",
    srcs = ["websocket_integration_test.cc"],
    data = [
        "//contrib/golang/filters/http/test/test_data:plugins.so",
    ],
    rbe_pool = "6gig",
    tags = [
        "cpu:3",
    ],
    deps = [
        "//contrib/golang/filters/http/source:config",
        "//source/common/http:header_map_lib",
        "//source/extensions/access_loggers/file:config",
        "//source/extensions/filters/http/buffer:config",
        "//test/integration:http_protocol_integration_lib",
        "//test/integration:websocket_integration_test_lib",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/config/bootstrap/v3:pkg_cc_proto",
        "@envoy_api//envoy/extensions/filters/network/http_connection_manager/v3:pkg_cc_proto",
    ],
)
