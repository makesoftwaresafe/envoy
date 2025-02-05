#include <memory>

#include "envoy/extensions/filters/http/header_to_metadata/v3/header_to_metadata.pb.h"

#include "source/common/common/base64.h"
#include "source/common/http/header_map_impl.h"
#include "source/common/protobuf/protobuf.h"
#include "source/extensions/filters/http/header_to_metadata/header_to_metadata_filter.h"
#include "source/extensions/filters/http/well_known_names.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/stream_info/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace HeaderToMetadataFilter {
namespace {

MATCHER_P(MapEq, rhs, "") {
  const ProtobufWkt::Struct& obj = arg;
  EXPECT_TRUE(!rhs.empty());
  for (auto const& entry : rhs) {
    EXPECT_EQ(obj.fields().at(entry.first).string_value(), entry.second);
  }
  return true;
}

MATCHER_P(MapEqNum, rhs, "") {
  const ProtobufWkt::Struct& obj = arg;
  EXPECT_TRUE(!rhs.empty());
  for (auto const& entry : rhs) {
    EXPECT_EQ(obj.fields().at(entry.first).number_value(), entry.second);
  }
  return true;
}

MATCHER_P(MapEqValue, rhs, "") {
  const ProtobufWkt::Struct& obj = arg;
  EXPECT_TRUE(!rhs.empty());
  for (auto const& entry : rhs) {
    EXPECT_TRUE(TestUtility::protoEqual(obj.fields().at(entry.first), entry.second));
  }
  return true;
}

} // namespace

class HeaderToMetadataTest : public testing::Test {
public:
  const std::string request_config_yaml = R"EOF(
request_rules:
  - header: x-version
    on_header_present:
      metadata_namespace: envoy.lb
      key: version
      type: STRING
    on_header_missing:
      metadata_namespace: envoy.lb
      key: default
      value: 'true'
      type: STRING
)EOF";

  absl::Status initializeFilter(const std::string& yaml) {
    envoy::extensions::filters::http::header_to_metadata::v3::Config config;
    TestUtility::loadFromYaml(yaml, config);
    absl::StatusOr<ConfigSharedPtr> config_or = Config::create(config, regex_engine_);
    RETURN_IF_NOT_OK_REF(config_or.status());
    config_ = std::move(*config_or);
    filter_ = std::make_shared<HeaderToMetadataFilter>(config_);
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
    return absl::OkStatus();
  }

  const Config* getConfig() { return filter_->getConfig(); }

  Regex::GoogleReEngine regex_engine_;
  ConfigSharedPtr config_;
  std::shared_ptr<HeaderToMetadataFilter> filter_;
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
  NiceMock<Envoy::StreamInfo::MockStreamInfo> req_info_;
};

/**
 * Basic use-case.
 */
TEST_F(HeaderToMetadataTest, BasicRequestTest) {
  EXPECT_TRUE(initializeFilter(request_config_yaml).ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", "0xdeadbeef"}};
  const std::map<std::string, std::string> expected = {{"version", "0xdeadbeef"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
  Http::MetadataMap metadata_map{{"metadata", "metadata"}};
  EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->decodeMetadata(metadata_map));
  Buffer::OwnedImpl data("data");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestRequestTrailerMapImpl incoming_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(incoming_trailers));
  filter_->onDestroy();
}

// Verify concatenation works.
TEST_F(HeaderToMetadataTest, BasicRequestDoubleHeadersTest) {
  EXPECT_TRUE(initializeFilter(request_config_yaml).ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", "foo"}, {"X-VERSION", "bar"}};
  const std::map<std::string, std::string> expected = {{"version", "foo,bar"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
  Http::MetadataMap metadata_map{{"metadata", "metadata"}};
  EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->decodeMetadata(metadata_map));
  Buffer::OwnedImpl data("data");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestRequestTrailerMapImpl incoming_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(incoming_trailers));
  filter_->onDestroy();
}

TEST_F(HeaderToMetadataTest, PerRouteOverride) {
  // Global config is empty.
  EXPECT_TRUE(initializeFilter("{}").ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", "0xdeadbeef"}};
  const std::map<std::string, std::string> expected = {{"version", "0xdeadbeef"}};

  // Setup per route config.
  envoy::extensions::filters::http::header_to_metadata::v3::Config config_proto;
  TestUtility::loadFromYaml(request_config_yaml, config_proto);
  ConfigSharedPtr per_route_config = *Config::create(config_proto, regex_engine_, true);
  EXPECT_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(_))
      .WillOnce(Return(per_route_config.get()));

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
  Http::MetadataMap metadata_map{{"metadata", "metadata"}};
  EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->decodeMetadata(metadata_map));
  Buffer::OwnedImpl data("data");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestRequestTrailerMapImpl incoming_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(incoming_trailers));
  filter_->onDestroy();
}

TEST_F(HeaderToMetadataTest, ConfigIsCached) {
  // Global config is empty.
  EXPECT_TRUE(initializeFilter("{}").ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", "0xdeadbeef"}};
  const std::map<std::string, std::string> expected = {{"version", "0xdeadbeef"}};

  // Setup per route config.
  envoy::extensions::filters::http::header_to_metadata::v3::Config config_proto;
  TestUtility::loadFromYaml(request_config_yaml, config_proto);
  ConfigSharedPtr per_route_config = *Config::create(config_proto, regex_engine_, true);
  EXPECT_CALL(*decoder_callbacks_.route_, mostSpecificPerFilterConfig(_))
      .WillOnce(Return(per_route_config.get()));

  EXPECT_TRUE(getConfig()->doRequest());
  EXPECT_TRUE(getConfig()->doRequest());
}

/**
 * X-version not set, the on missing value should be set.
 */
TEST_F(HeaderToMetadataTest, DefaultEndpointsTest) {
  EXPECT_TRUE(initializeFilter(request_config_yaml).ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-FOO", "bar"}};
  const std::map<std::string, std::string> expected = {{"default", "true"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
}

/**
 * Test that private headers get removed.
 */
TEST_F(HeaderToMetadataTest, HeaderRemovedTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: STRING
    remove: true
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", "1"}};
  const std::map<std::string, std::string> expected = {{"auth", "1"}};
  Http::TestResponseHeaderMapImpl empty_headers;

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEq(expected)));
  Http::TestResponseHeaderMapImpl continue_response{{":status", "100"}};
  EXPECT_EQ(Http::Filter1xxHeadersStatus::Continue, filter_->encode1xxHeaders(continue_response));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
  EXPECT_EQ(empty_headers, incoming_headers);
  Http::MetadataMap metadata_map{{"metadata", "metadata"}};
  EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->encodeMetadata(metadata_map));
  Buffer::OwnedImpl data("data");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data, false));
  Http::TestResponseTrailerMapImpl incoming_trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(incoming_trailers));
}

/**
 * Test the value gets written as a number.
 */
TEST_F(HeaderToMetadataTest, NumberTypeTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: NUMBER
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", "1"}};
  std::map<std::string, int> expected = {{"auth", 1}};
  Http::TestResponseHeaderMapImpl empty_headers;

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEqNum(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Test the Base64 encoded value gets written as a string.
 */
TEST_F(HeaderToMetadataTest, StringTypeInBase64UrlTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: STRING
      encode: BASE64
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  std::string data = "Non-ascii-characters";
  const auto encoded = Base64::encode(data.c_str(), data.size());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", encoded}};
  const std::map<std::string, std::string> expected = {{"auth", data}};
  Http::TestResponseHeaderMapImpl empty_headers;

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Test the Base64 encoded protobuf value gets written as a protobuf value.
 */
TEST_F(HeaderToMetadataTest, ProtobufValueTypeInBase64UrlTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: PROTOBUF_VALUE
      encode: BASE64
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());

  ProtobufWkt::Value value;
  auto* s = value.mutable_struct_value();

  ProtobufWkt::Value v;
  v.set_string_value("blafoo");
  (*s->mutable_fields())["k1"] = v;
  v.set_number_value(2019.07);
  (*s->mutable_fields())["k2"] = v;
  v.set_bool_value(true);
  (*s->mutable_fields())["k3"] = v;

  std::string data;
  ASSERT_TRUE(value.SerializeToString(&data));
  const auto encoded = Base64::encode(data.c_str(), data.size());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", encoded}};
  std::map<std::string, ProtobufWkt::Value> expected = {{"auth", value}};

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEqValue(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Test bad Base64 encoding is not written.
 */
TEST_F(HeaderToMetadataTest, ProtobufValueTypeInBadBase64UrlTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: PROTOBUF_VALUE
      encode: BASE64
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", "invalid"}};

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Test the bad protobuf value is not written.
 */
TEST_F(HeaderToMetadataTest, BadProtobufValueTypeInBase64UrlTest) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-authenticated
    on_header_present:
      key: auth
      type: PROTOBUF_VALUE
      encode: BASE64
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  std::string data = "invalid";
  const auto encoded = Base64::encode(data.c_str(), data.size());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-authenticated", encoded}};

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Headers not present.
 */
TEST_F(HeaderToMetadataTest, HeaderNotPresent) {
  const std::string config = R"EOF(
request_rules:
  - header: x-version
    on_header_present:
      metadata_namespace: envoy.lb
      key: version
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl incoming_headers;

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
}

/**
 * Two headers match.
 */
TEST_F(HeaderToMetadataTest, MultipleHeadersMatch) {
  const std::string python_yaml = R"EOF(
request_rules:
  - header: x-version
    on_header_present:
      key: version
      metadata_namespace: envoy.lb
      type: STRING
  - header: x-python-version
    on_header_present:
      key: python_version
      metadata_namespace: envoy.lb
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(python_yaml).ok());
  Http::TestRequestHeaderMapImpl incoming_headers{
      {"X-VERSION", "v4.0"},
      {"X-PYTHON-VERSION", "3.7"},
      {"X-IGNORE", "nothing"},
  };
  const std::map<std::string, std::string> expected = {{"version", "v4.0"},
                                                       {"python_version", "3.7"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
}

/**
 * No header value.
 */
TEST_F(HeaderToMetadataTest, EmptyHeaderValue) {
  EXPECT_TRUE(initializeFilter(request_config_yaml).ok());
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", ""}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
}

/**
 * Header value too long.
 */
TEST_F(HeaderToMetadataTest, HeaderValueTooLong) {
  EXPECT_TRUE(initializeFilter(request_config_yaml).ok());
  auto length = Envoy::Extensions::HttpFilters::HeaderToMetadataFilter::MAX_HEADER_VALUE_LEN + 1;
  Http::TestRequestHeaderMapImpl incoming_headers{{"X-VERSION", std::string(length, 'x')}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(incoming_headers, false));
}

/**
 * Ignore the header's value, use a constant value.
 */
TEST_F(HeaderToMetadataTest, IgnoreHeaderValueUseConstant) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - header: x-something
    on_header_present:
      key: something
      value: else
      type: STRING
    remove: true
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"x-something", "thing"}};
  const std::map<std::string, std::string> expected = {{"something", "else"}};
  Http::TestResponseHeaderMapImpl empty_headers;

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
  EXPECT_EQ(empty_headers, incoming_headers);
}

/**
 * Rules with no on_header{present,missing} fields should be rejected.
 */
TEST_F(HeaderToMetadataTest, RejectInvalidRuleOnRequestRules) {
  const std::string config = R"EOF(
request_rules:
  - header: x-something
)EOF";
  auto expected = "header to metadata filter: rule for header 'x-something' has neither "
                  "`on_header_present` nor `on_header_missing` set";
  auto status = initializeFilter(config);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), expected);
}

TEST_F(HeaderToMetadataTest, RejectInvalidRuleOnResponseRules) {
  const std::string config = R"EOF(
response_rules:
  - header: x-something
)EOF";
  auto expected = "header to metadata filter: rule for header 'x-something' has neither "
                  "`on_header_present` nor `on_header_missing` set";
  auto status = initializeFilter(config);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), expected);
}

TEST_F(HeaderToMetadataTest, PerRouteEmtpyRules) {
  envoy::extensions::filters::http::header_to_metadata::v3::Config config_proto;
  auto expected = "header_to_metadata_filter: Per filter configs must at "
                  "least specify either request or response rules";
  auto create_or = Config::create(config_proto, regex_engine_, true);
  EXPECT_FALSE(create_or.ok());
  EXPECT_EQ(create_or.status().message(), expected);
}

/**
 * Invalid empty header or cookie should be rejected.
 */
TEST_F(HeaderToMetadataTest, RejectEmptyHeader) {
  const std::string config = R"EOF(
request_rules:
  - header: ""

)EOF";
  auto expected = "One of Cookie or Header option needs to be specified";
  auto status = initializeFilter(config);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), expected);
}

/**
 * Rules with both header and cookie fields should be rejected.
 */
TEST_F(HeaderToMetadataTest, RejectBothCookieHeader) {
  const std::string config = R"EOF(
request_rules:
  - header: x-something
    cookie: something-else
    on_header_present:
      key: something
      value: else
      type: STRING
    remove: false

)EOF";
  auto expected = "Cannot specify both header and cookie in the same rule";
  auto status = initializeFilter(config);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), expected);
}

/**
 * Rules with remove field should be rejected in case of a cookie.
 */
TEST_F(HeaderToMetadataTest, RejectRemoveForCookie) {
  const std::string config = R"EOF(
request_rules:
  - cookie: cookie
    on_header_present:
      metadata_namespace: envoy.lb
      key: version
      type: STRING
    remove: true
)EOF";
  auto expected = "Cannot specify remove for cookie";
  auto status = initializeFilter(config);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.message(), expected);
}

/**
 * Empty values not added to metadata.
 */
TEST_F(HeaderToMetadataTest, NoEmptyValues) {
  const std::string config = R"EOF(
request_rules:
  - header: x-version
    on_header_present:
      metadata_namespace: envoy.lb
      key: version
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl headers{{"x-version", ""}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

/**
 * Regex substitution on header value.
 */
TEST_F(HeaderToMetadataTest, RegexSubstitution) {
  const std::string config = R"EOF(
request_rules:
  - header: :path
    on_header_present:
      metadata_namespace: envoy.lb
      key: cluster
      regex_value_rewrite:
        pattern:
          google_re2: {}
          regex: "^/(cluster[\\d\\w-]+)/?.*$"
        substitution: "\\1"
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());

  // Match with additional path elements.
  {
    Http::TestRequestHeaderMapImpl headers{{":path", "/cluster-prod-001/x/y"}};
    const std::map<std::string, std::string> expected = {{"cluster", "cluster-prod-001"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // Match with no additional path elements.
  {
    Http::TestRequestHeaderMapImpl headers{{":path", "/cluster-prod-001"}};
    const std::map<std::string, std::string> expected = {{"cluster", "cluster-prod-001"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // No match.
  {
    Http::TestRequestHeaderMapImpl headers{{":path", "/foo"}};
    const std::map<std::string, std::string> expected = {{"cluster", "/foo"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // No match with additional path elements.
  {
    Http::TestRequestHeaderMapImpl headers{{":path", "/foo/bar?x=2"}};
    const std::map<std::string, std::string> expected = {{"cluster", "/foo/bar?x=2"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }
}

/**
 * Missing case is not executed when header is present.
 */
TEST_F(HeaderToMetadataTest, NoMissingWhenHeaderIsPresent) {
  const std::string config = R"EOF(
request_rules:
  - header: x-version
    on_header_missing:
      metadata_namespace: envoy.lb
      key: version
      value: some_value
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl headers{{"x-version", "19"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

/**
 * on header missing case with no header data
 */

TEST_F(HeaderToMetadataTest, OnMissingWhenHeaderIsPresent) {
  const std::string config = R"EOF(
request_rules:
  - header: x-version
    on_header_missing:
      metadata_namespace: envoy.lb
      key: version
      value: some_value
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl headers{{"x-version", ""}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

/**
 * on header present case, when the regex replacement turns the header into an empty string
 */
TEST_F(HeaderToMetadataTest, HeaderIsPresentButRegexEmptiesIt) {
  const std::string config = R"EOF(
request_rules:
  - header: x-version
    on_header_present:
      metadata_namespace: envoy.lb
      key: cluster
      regex_value_rewrite:
        pattern:
          google_re2: {}
          regex: "^foo"
        substitution: ""
    on_header_missing:
      metadata_namespace: envoy.lb
      key: version
      value: some_value
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl headers{{"x-version", "foo"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata(_, _)).Times(0);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

/**
 * cookie value extracted and stored
 */
TEST_F(HeaderToMetadataTest, CookieValueUsed) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - cookie: bar
    on_header_present:
      key: bar
      type: STRING
    remove: false
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"cookie", "bar=foo"}};
  const std::map<std::string, std::string> expected = {{"bar", "foo"}};

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * Ignore the cookie's value, use a given constant value.
 */
TEST_F(HeaderToMetadataTest, IgnoreCookieValueUseConstant) {
  const std::string response_config_yaml = R"EOF(
response_rules:
  - cookie: meh
    on_header_present:
      key: meh
      value: some_value
      type: STRING
    remove: false
)EOF";
  EXPECT_TRUE(initializeFilter(response_config_yaml).ok());
  Http::TestResponseHeaderMapImpl incoming_headers{{"cookie", "meh=foo"}};
  const std::map<std::string, std::string> expected = {{"meh", "some_value"}};

  EXPECT_CALL(encoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_,
              setDynamicMetadata(HttpFilterNames::get().HeaderToMetadata, MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(incoming_headers, false));
}

/**
 * No cookie value, no metadata
 */
TEST_F(HeaderToMetadataTest, NoCookieValue) {
  const std::string config = R"EOF(
request_rules:
  - cookie: foo
    on_header_missing:
      metadata_namespace: envoy.lb
      key: foo
      value: some_value
      type: STRING
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());
  Http::TestRequestHeaderMapImpl headers{{"cookie", ""}};
  const std::map<std::string, std::string> expected = {{"foo", "some_value"}};

  EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
  EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
}

/**
 * Regex substitution on cookie value.
 */
TEST_F(HeaderToMetadataTest, CookieRegexSubstitution) {
  const std::string config = R"EOF(
request_rules:
  - cookie: foo
    on_header_present:
      metadata_namespace: envoy.lb
      key: cluster
      regex_value_rewrite:
        pattern:
          google_re2: {}
          regex: "^(cluster[\\d\\w-]+)$"
        substitution: "\\1 matched"
)EOF";
  EXPECT_TRUE(initializeFilter(config).ok());

  // match.
  {
    Http::TestRequestHeaderMapImpl headers{{"cookie", "foo=cluster-prod-001"}};
    const std::map<std::string, std::string> expected = {{"cluster", "cluster-prod-001 matched"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }

  // No match.
  {
    Http::TestRequestHeaderMapImpl headers{{"cookie", "foo=cluster"}};
    const std::map<std::string, std::string> expected = {{"cluster", "cluster"}};

    EXPECT_CALL(decoder_callbacks_, streamInfo()).WillRepeatedly(ReturnRef(req_info_));
    EXPECT_CALL(req_info_, setDynamicMetadata("envoy.lb", MapEq(expected)));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  }
}

} // namespace HeaderToMetadataFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
