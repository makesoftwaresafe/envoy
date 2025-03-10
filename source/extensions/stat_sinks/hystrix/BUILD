load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_extension",
    "envoy_cc_library",
    "envoy_extension_package",
)

licenses(["notice"])  # Apache 2

# Stats sink for the basic version of the hystrix protocol (https://github.com/b/hystrix_spec).

envoy_extension_package()

envoy_cc_extension(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        ":hystrix_lib",
        "//envoy/registry",
        "//source/common/network:address_lib",
        "//source/server:configuration_lib",
        "@envoy_api//envoy/config/metrics/v3:pkg_cc_proto",
    ],
)

envoy_cc_library(
    name = "hystrix_lib",
    srcs = ["hystrix.cc"],
    hdrs = ["hystrix.h"],
    deps = [
        "//envoy/server:admin_interface",
        "//envoy/server:instance_interface",
        "//envoy/stats:stats_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:logger_lib",
        "//source/common/config:well_known_names",
        "//source/common/http:headers_lib",
        "//source/common/stats:symbol_table_lib",
        "//source/common/stats:utility_lib",
    ],
)
