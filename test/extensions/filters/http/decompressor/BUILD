load(
    "//bazel:envoy_build_system.bzl",
    "envoy_package",
)
load(
    "//test/extensions:extensions_build_system.bzl",
    "envoy_extension_cc_test",
)

licenses(["notice"])  # Apache 2

envoy_package()

envoy_extension_cc_test(
    name = "decompressor_filter_test",
    srcs = ["decompressor_filter_test.cc"],
    extension_names = ["envoy.filters.http.decompressor"],
    rbe_pool = "6gig",
    deps = [
        "//source/common/http:headers_lib",
        "//source/common/protobuf:utility_lib",
        "//source/extensions/compression/gzip/decompressor:config",
        "//source/extensions/filters/http/decompressor:config",
        "//test/mocks/compression/decompressor:decompressor_mocks",
        "//test/mocks/http:http_mocks",
        "//test/mocks/protobuf:protobuf_mocks",
        "//test/mocks/runtime:runtime_mocks",
        "//test/test_common:utility_lib",
        "@envoy_api//envoy/extensions/filters/http/decompressor/v3:pkg_cc_proto",
    ],
)

envoy_extension_cc_test(
    name = "decompressor_filter_integration_test",
    size = "large",
    srcs = [
        "decompressor_filter_integration_test.cc",
    ],
    extension_names = ["envoy.filters.http.decompressor"],
    rbe_pool = "6gig",
    deps = [
        "//source/extensions/compression/gzip/compressor:config",
        "//source/extensions/compression/gzip/decompressor:config",
        "//source/extensions/filters/http/decompressor:config",
        "//test/integration:http_integration_lib",
        "//test/integration/filters:encoder_decoder_buffer_filter_lib",
        "//test/mocks/server:factory_context_mocks",
        "//test/test_common:simulated_time_system_lib",
        "//test/test_common:utility_lib",
    ],
)
