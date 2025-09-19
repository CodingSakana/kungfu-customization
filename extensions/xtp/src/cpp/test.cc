#include "serialize_xtp.h"

// clang++ -std=c++20 -O3 -pthread -Inode_modules/@kungfu-trader/kfx-broker-xtp-demo/__kungfulibs__/xtp/v2.2.37.4/include ./extensions/xtp/src/cpp/test.cc -o ./extensions/xtp/src/cpp/test

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace kungfu::wingchun::xtp::testdata {
namespace {

constexpr const char *kTickerBase = "\xb9\xc9\xc6\xb1";  // "股票" in GBK
constexpr const char *kNameBase = "\xc3\xfb\xb3\xc6";    // "名称" in GBK
constexpr const char *kExecBase = "\xd6\xb4\xd0\xd0";    // "执行" in GBK
constexpr const char *kOrderBase = "\xb6\xa9\xb5\xa5";   // "订单" in GBK
constexpr const char *kBranchBase = "\xb7\xd6\xd6\xa7";  // "分支" in GBK

template <std::size_t N>
void set_char_field(char (&dest)[N], const std::string &value) {
  std::snprintf(dest, N, "%s", value.c_str());
}

std::string with_index(const char *prefix, std::size_t index) {
  return std::string(prefix) + std::to_string(index);
}

XTP_MARKET_TYPE sample_market(std::size_t index) {
  static constexpr XTP_MARKET_TYPE markets[] = {XTP_MKT_SZ_A, XTP_MKT_SH_A, XTP_MKT_BJ_A};
  return markets[index % (sizeof(markets) / sizeof(markets[0]))];
}

XTP_EXCHANGE_TYPE sample_exchange(std::size_t index) {
  static constexpr XTP_EXCHANGE_TYPE exchanges[] = {XTP_EXCHANGE_SH, XTP_EXCHANGE_SZ, XTP_EXCHANGE_NQ};
  return exchanges[index % (sizeof(exchanges) / sizeof(exchanges[0]))];
}

XTP_SIDE_TYPE sample_side(std::size_t index) {
  return index % 2 == 0 ? XTP_SIDE_BUY : XTP_SIDE_SELL;
}

XTP_POSITION_EFFECT_TYPE sample_position_effect(std::size_t index) {
  switch (index % 3) {
  case 0:
    return XTP_POSITION_EFFECT_OPEN;
  case 1:
    return XTP_POSITION_EFFECT_CLOSE;
  default:
    return XTP_POSITION_EFFECT_CLOSETODAY;
  }
}

XTP_BUSINESS_TYPE sample_business_type(std::size_t index) {
  static constexpr XTP_BUSINESS_TYPE types[] = {XTP_BUSINESS_TYPE_CASH, XTP_BUSINESS_TYPE_MARGIN,
                                                 XTP_BUSINESS_TYPE_OPTION};
  return types[index % (sizeof(types) / sizeof(types[0]))];
}

XTP_ACCOUNT_TYPE sample_account(std::size_t index) {
  static constexpr XTP_ACCOUNT_TYPE types[] = {XTP_ACCOUNT_NORMAL, XTP_ACCOUNT_CREDIT, XTP_ACCOUNT_DERIVE};
  return types[index % (sizeof(types) / sizeof(types[0]))];
}

XTP_POSITION_DIRECTION_TYPE sample_direction(std::size_t index) {
  static constexpr XTP_POSITION_DIRECTION_TYPE directions[] = {XTP_POSITION_DIRECTION_LONG,
                                                                XTP_POSITION_DIRECTION_SHORT,
                                                                XTP_POSITION_DIRECTION_NET};
  return directions[index % (sizeof(directions) / sizeof(directions[0]))];
}

XTP_POSITION_SECURITY_TYPE sample_security_type(std::size_t index) {
  static constexpr XTP_POSITION_SECURITY_TYPE types[] = {XTP_POSITION_SECURITY_NORMAL,
                                                          XTP_POSITION_SECURITY_PLACEMENT};
  return types[index % (sizeof(types) / sizeof(types[0]))];
}

template <typename T, typename Generator>
std::vector<T> make_samples(std::size_t count, Generator &&generator) {
  std::vector<T> samples;
  samples.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    samples.emplace_back(generator(i));
  }
  return samples;
}

} // namespace

XTPOrderInsertInfo make_order_insert_info(std::size_t index) {
  XTPOrderInsertInfo info{};
  info.order_xtp_id = 10'000 + index;
  info.order_client_id = static_cast<uint32_t>(1'000 + index);
  set_char_field(info.ticker, with_index(kTickerBase, index));
  info.market = sample_market(index);
  info.price = 10.0 + static_cast<double>(index);
  info.stop_price = 9.5 + static_cast<double>(index);
  info.quantity = 100 + static_cast<int64_t>(index);
  info.price_type = XTP_PRICE_LIMIT;
  info.side = sample_side(index);
  info.position_effect = sample_position_effect(index);
  info.reserved1 = static_cast<uint8_t>(index % 0xFF);
  info.reserved2 = static_cast<uint8_t>((index + 1) % 0xFF);
  info.business_type = sample_business_type(index);
  return info;
}

XTPQueryOrderRsp make_query_order_rsp(std::size_t index) {
  XTPQueryOrderRsp rsp{};
  rsp.order_xtp_id = 20'000 + index;
  rsp.order_client_id = static_cast<uint32_t>(2'000 + index);
  rsp.order_cancel_client_id = static_cast<uint32_t>(3'000 + index);
  rsp.order_cancel_xtp_id = 30'000 + index;
  set_char_field(rsp.ticker, with_index(kTickerBase, index));
  rsp.market = sample_market(index);
  rsp.price = 11.0 + static_cast<double>(index);
  rsp.quantity = 200 + static_cast<int64_t>(index);
  rsp.price_type = XTP_PRICE_BEST5_OR_LIMIT;
  rsp.side = sample_side(index);
  rsp.position_effect = sample_position_effect(index);
  rsp.reserved1 = static_cast<uint8_t>((index + 2) % 0xFF);
  rsp.reserved2 = static_cast<uint8_t>((index + 3) % 0xFF);
  rsp.business_type = sample_business_type(index);
  rsp.qty_traded = 150 + static_cast<int64_t>(index);
  rsp.qty_left = 50 + static_cast<int64_t>(index % 10);
  rsp.insert_time = 20230101090000000LL + static_cast<int64_t>(index);
  rsp.update_time = rsp.insert_time + 1'000;
  rsp.cancel_time = rsp.insert_time + 2'000;
  rsp.trade_amount = rsp.price * static_cast<double>(rsp.qty_traded);
  set_char_field(rsp.order_local_id, with_index(kOrderBase, index));
  rsp.order_status = XTP_ORDER_STATUS_PARTTRADEDQUEUEING;
  rsp.order_submit_status = XTP_ORDER_SUBMIT_STATUS_INSERT_ACCEPTED;
  rsp.order_type = XTP_ORDT_Normal;
  return rsp;
}

XTPTradeReport make_trade_report(std::size_t index) {
  XTPTradeReport report{};
  report.order_xtp_id = 40'000 + index;
  report.order_client_id = static_cast<uint32_t>(4'000 + index);
  set_char_field(report.ticker, with_index(kTickerBase, index));
  report.market = sample_market(index);
  report.local_order_id = 50'000 + index;
  set_char_field(report.exec_id, with_index(kExecBase, index));
  report.price = 12.5 + static_cast<double>(index);
  report.quantity = 300 + static_cast<int64_t>(index);
  report.trade_time = 20230101090100000LL + static_cast<int64_t>(index);
  report.trade_amount = report.price * static_cast<double>(report.quantity);
  report.report_index = 60'000 + index;
  set_char_field(report.order_exch_id, with_index(kOrderBase, index));
  report.trade_type = XTP_TRDT_COMMON;
  report.side = sample_side(index);
  report.position_effect = sample_position_effect(index);
  report.reserved1 = static_cast<uint8_t>((index + 4) % 0xFF);
  report.reserved2 = static_cast<uint8_t>((index + 5) % 0xFF);
  report.business_type = sample_business_type(index);
  set_char_field(report.branch_pbu, with_index(kBranchBase, index));
  return report;
}

XTPOrderCancelInfo make_order_cancel_info(std::size_t index) {
  XTPOrderCancelInfo info{};
  info.order_cancel_xtp_id = 70'000 + index;
  info.order_xtp_id = 20'000 + index;
  return info;
}

XTPQueryAssetRsp make_asset_rsp(std::size_t index) {
  XTPQueryAssetRsp asset{};
  asset.total_asset = 1'000'000.0 + static_cast<double>(index * 1'000);
  asset.buying_power = 500'000.0 + static_cast<double>(index * 500);
  asset.security_asset = 250'000.0 + static_cast<double>(index * 250);
  asset.fund_buy_amount = 120'000.0 + static_cast<double>(index * 100);
  asset.fund_buy_fee = 500.0 + static_cast<double>(index);
  asset.fund_sell_amount = 80'000.0 + static_cast<double>(index * 80);
  asset.fund_sell_fee = 400.0 + static_cast<double>(index);
  asset.withholding_amount = 5'000.0 + static_cast<double>(index * 10);
  asset.account_type = sample_account(index);
  asset.frozen_margin = 2'000.0 + static_cast<double>(index * 5);
  asset.frozen_exec_cash = 1'000.0 + static_cast<double>(index * 2);
  asset.frozen_exec_fee = 200.0 + static_cast<double>(index);
  asset.pay_later = 100.0 + static_cast<double>(index);
  asset.preadva_pay = 50.0 + static_cast<double>(index);
  asset.orig_banlance = 30'000.0 + static_cast<double>(index * 30);
  asset.banlance = 31'000.0 + static_cast<double>(index * 30);
  asset.deposit_withdraw = static_cast<double>(index * 100);
  asset.trade_netting = static_cast<double>(index * 200);
  asset.captial_asset = 15'000.0 + static_cast<double>(index * 10);
  asset.force_freeze_amount = 1'500.0 + static_cast<double>(index * 5);
  asset.preferred_amount = 750.0 + static_cast<double>(index * 3);
  asset.repay_stock_aval_banlance = 2'500.0 + static_cast<double>(index * 4);
  asset.fund_order_data_charges = 120.0 + static_cast<double>(index);
  asset.fund_cancel_data_charges = 60.0 + static_cast<double>(index);
  asset.exchange_cur_risk_degree = 0.1 + static_cast<double>(index) * 0.01;
  asset.company_cur_risk_degree = 0.2 + static_cast<double>(index) * 0.01;
  for (std::size_t i = 0; i < sizeof(asset.unknown) / sizeof(asset.unknown[0]); ++i) {
    asset.unknown[i] = static_cast<uint64_t>(index + i);
  }
  return asset;
}

XTPQueryStkPositionRsp make_stk_position_rsp(std::size_t index) {
  XTPQueryStkPositionRsp position{};
  set_char_field(position.ticker, with_index(kTickerBase, index));
  set_char_field(position.ticker_name, with_index(kNameBase, index));
  position.market = sample_market(index);
  position.total_qty = 1'000 + static_cast<int64_t>(index * 10);
  position.sellable_qty = 800 + static_cast<int64_t>(index * 8);
  position.avg_price = 15.0 + static_cast<double>(index);
  position.unrealized_pnl = 500.0 + static_cast<double>(index * 5);
  position.yesterday_position = 900 + static_cast<int64_t>(index * 9);
  position.purchase_redeemable_qty = 100 + static_cast<int64_t>(index);
  position.position_direction = sample_direction(index);
  position.position_security_type = sample_security_type(index);
  position.executable_option = 50 + static_cast<int64_t>(index);
  position.lockable_position = 40 + static_cast<int64_t>(index);
  position.executable_underlying = 30 + static_cast<int64_t>(index);
  position.locked_position = 20 + static_cast<int64_t>(index);
  position.usable_locked_position = 10 + static_cast<int64_t>(index);
  position.profit_price = 16.0 + static_cast<double>(index);
  position.buy_cost = 9'000.0 + static_cast<double>(index * 90);
  position.profit_cost = 500.0 + static_cast<double>(index * 5);
  position.market_value = 12'000.0 + static_cast<double>(index * 100);
  position.margin = 1'200.0 + static_cast<double>(index * 10);
  position.last_buy_cost = 8'000.0 + static_cast<double>(index * 80);
  position.last_profit_cost = 400.0 + static_cast<double>(index * 4);
  for (std::size_t i = 0; i < sizeof(position.unknown) / sizeof(position.unknown[0]); ++i) {
    position.unknown[i] = static_cast<uint64_t>(index + i);
  }
  return position;
}

XTPMarketDataStruct make_market_data(std::size_t index) {
  XTPMarketDataStruct data{};
  data.exchange_id = sample_exchange(index);
  set_char_field(data.ticker, with_index(kTickerBase, index));
  data.last_price = 20.0 + static_cast<double>(index);
  data.pre_close_price = data.last_price - 0.5;
  data.open_price = data.last_price - 0.2;
  data.high_price = data.last_price + 0.5;
  data.low_price = data.last_price - 0.5;
  data.close_price = data.last_price + 0.1;
  data.pre_total_long_positon = 10'000 + static_cast<int64_t>(index * 10);
  data.total_long_positon = 11'000 + static_cast<int64_t>(index * 12);
  data.pre_settl_price = data.last_price - 0.3;
  data.settl_price = data.last_price + 0.3;
  data.upper_limit_price = data.last_price + 1.0;
  data.lower_limit_price = data.last_price - 1.0;
  data.pre_delta = 0.1 + static_cast<double>(index) * 0.01;
  data.curr_delta = 0.2 + static_cast<double>(index) * 0.01;
  data.data_time = 20230101090200000LL + static_cast<int64_t>(index);
  data.qty = 1'000'000 + static_cast<int64_t>(index * 1'000);
  data.turnover = data.last_price * static_cast<double>(data.qty);
  data.avg_price = data.turnover / (data.qty == 0 ? 1.0 : static_cast<double>(data.qty));
  for (std::size_t level = 0; level < 10; ++level) {
    data.bid[level] = data.last_price - 0.01 * static_cast<double>(level + 1);
    data.ask[level] = data.last_price + 0.01 * static_cast<double>(level + 1);
    data.bid_qty[level] = 10'000 + static_cast<int64_t>(index * 100 + level);
    data.ask_qty[level] = 9'000 + static_cast<int64_t>(index * 90 + level);
  }
  data.trades_count = 5'000 + static_cast<int64_t>(index * 50);
  set_char_field(data.ticker_status, "ACTIVE");
  data.data_type = XTP_MARKETDATA_ACTUAL;
  data.data_type_v2 = XTP_MARKETDATA_V2_ACTUAL;
  return data;
}

std::vector<XTPOrderInsertInfo> make_order_insert_infos(std::size_t count) {
  return make_samples<XTPOrderInsertInfo>(count, make_order_insert_info);
}

std::vector<XTPQueryOrderRsp> make_query_order_rsps(std::size_t count) {
  return make_samples<XTPQueryOrderRsp>(count, make_query_order_rsp);
}

std::vector<XTPTradeReport> make_trade_reports(std::size_t count) {
  return make_samples<XTPTradeReport>(count, make_trade_report);
}

std::vector<XTPOrderCancelInfo> make_order_cancel_infos(std::size_t count) {
  return make_samples<XTPOrderCancelInfo>(count, make_order_cancel_info);
}

std::vector<XTPQueryAssetRsp> make_asset_rsps(std::size_t count) {
  return make_samples<XTPQueryAssetRsp>(count, make_asset_rsp);
}

std::vector<XTPQueryStkPositionRsp> make_stk_position_rsps(std::size_t count) {
  return make_samples<XTPQueryStkPositionRsp>(count, make_stk_position_rsp);
}

std::vector<XTPMarketDataStruct> make_market_data_samples(std::size_t count) {
  return make_samples<XTPMarketDataStruct>(count, make_market_data);
}

} // namespace kungfu::wingchun::xtp::testdata

int main(int argc, char **argv) {
  using kungfu::wingchun::xtp::to_string;
  using namespace kungfu::wingchun::xtp::testdata;

  try {
    auto file_logger = spdlog::basic_logger_st("xtp_perf", "./extensions/xtp/src/cpp/xtp_perf.log");
    spdlog::set_default_logger(file_logger);
    spdlog::flush_on(spdlog::level::info);
  } catch (const spdlog::spdlog_ex &ex) {
    spdlog::error("Failed to set file logger: {}", ex.what());
  }

  std::size_t count = 1'000;
  if (argc > 1) {
    const auto parsed = std::strtoull(argv[1], nullptr, 10);
    if (parsed > 0) {
      count = static_cast<std::size_t>(parsed);
    }
  }

  std::size_t total_bytes = 0;
  auto benchmark = [&](const char *label, auto &&factory) {
    auto samples = factory(count);
    const auto start = std::chrono::high_resolution_clock::now();
    std::size_t bytes = 0;
    for (const auto &sample : samples) {
      bytes += to_string(sample).size();
      spdlog::info("{}: {}", label, to_string(sample));
    }
    const auto stop = std::chrono::high_resolution_clock::now();
    const double us = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
    total_bytes += bytes;
    spdlog::info("{}: {} us per item (count={}, bytes={})", label, us / count, count, bytes);
    spdlog::info("=======================================");
  };

  benchmark("XTPOrderInsertInfo", make_order_insert_infos);
  benchmark("XTPQueryOrderRsp", make_query_order_rsps);
  benchmark("XTPTradeReport", make_trade_reports);
  benchmark("XTPOrderCancelInfo", make_order_cancel_infos);
  benchmark("XTPQueryAssetRsp", make_asset_rsps);
  benchmark("XTPQueryStkPositionRsp", make_stk_position_rsps);
  benchmark("XTPMarketDataStruct", make_market_data_samples);

  spdlog::info("Total bytes serialized: {}", total_bytes);
  return 0;
}
