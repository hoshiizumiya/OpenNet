#pragma once
#include "pch.h"
#include <libtorrent/session.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_handle.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/alert_types.hpp>
#include <libtorrent/settings_pack.hpp>
#include <chrono>
#include <thread>
#include <iostream>

namespace lt = libtorrent;
//
//void init_libtorrent()
//{
//    // 1) 基础设置
//    lt::settings_pack pack;
//    // 监听端口（同时用于 uTP/UDP）
//    pack.set_str(lt::settings_pack::listen_interfaces, "0.0.0.0:6881,[::]:6881");
//    // 启用 DHT / LSD / UPnP / NAT-PMP
//    pack.set_bool(lt::settings_pack::enable_dht, true);
//    pack.set_bool(lt::settings_pack::enable_lsd, true);
//    pack.set_bool(lt::settings_pack::enable_upnp, true);
//    pack.set_bool(lt::settings_pack::enable_natpmp, true);
//    // 允许 TCP + uTP
//    pack.set_bool(lt::settings_pack::enable_incoming_tcp, true);
//    pack.set_bool(lt::settings_pack::enable_outgoing_tcp, true);
//    pack.set_bool(lt::settings_pack::enable_incoming_utp, true);
//    pack.set_bool(lt::settings_pack::enable_outgoing_utp, true);
//    ////（可选）加密策略：允许 MSE
//    //pack.set_int(lt::settings_pack::in_enc_policy, lt::enc_policy::pe_enabled);
//    //pack.set_int(lt::settings_pack::out_enc_policy, lt::enc_policy::pe_enabled);
//    //pack.set_int(lt::settings_pack::allowed_enc_level, lt::enc_level::both);
//
//    lt::session ses(pack);
//
//    //// 2) 添加磁力任务
//    //std::string magnet = "magnet:?xt=urn:btih:..."; // 放你的磁力链接
//    //lt::add_torrent_params atp = lt::parse_magnet_uri(magnet);
//    //atp.save_path = "D:/Downloads"; // 保存目录
//    //lt::torrent_handle h = ses.add_torrent(atp);
//
//    // 3) 主循环：拉取 alerts 显示进度
//    using namespace std::chrono_literals;
//    while (true)
//    {
//        std::vector<lt::alert*> alerts;
//        ses.pop_alerts(&alerts);
//        for (lt::alert* a : alerts)
//        {
//            if (auto st = lt::alert_cast<lt::state_update_alert>(a))
//            {
//                for (auto const& s : st->status)
//                {
//                    std::cout << "progress: " << int(s.progress_ppm / 10000) << "% "
//                        << "down: " << (s.download_rate / 1000) << " kB/s "
//                        << "up: " << (s.upload_rate / 1000) << " kB/s\r";
//                }
//            }
//            else if (auto tf = lt::alert_cast<lt::torrent_finished_alert>(a))
//            {
//                std::cout << "\nFinished: " << tf->handle.status().name << "\n";
//            }
//            else if (auto e = lt::alert_cast<lt::error_alert>(a))
//            {
//                std::cerr << "\nError: " << e->message() << "\n";
//            }
//        }
//        // 请求一次状态更新（异步，下一轮会收到 state_update_alert）
//        ses.post_torrent_updates();
//        std::this_thread::sleep_for(500ms);
//    }
//}