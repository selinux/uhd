/*
 * =====================================================================================
 *
 *       Filename:  print_rw_register.cpp
 *
 *    Description:  test read/write acces to user setting register (LoRa)
 *
 *        Version:  1.0
 *        Created:  28.05.2017 18:55:41
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Sebastien Chassot (sinux), sebastien.chassot@etu.hesge.ch
 *        Company:  HES-SO hepia section ITI (soir)
 *
 * =====================================================================================
 */

//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <complex>
#include "../lib/usrp/cores/user_settings_core_3000.hpp"

#define NB_TESTS 60

namespace po = boost::program_options;

int UHD_SAFE_MAIN(int argc, char *argv[]){
    uhd::set_thread_priority_safe();

    //variables to be set by po
    std::string args;
    std::string args_board0 = "name=hepiaB200";
    std::string args_board1 = "name=sinuxB200";
    std::string wire;
    int test_time = 0;

    std::string time_source;

    double seconds_in_future;
    size_t total_num_samps;
    double rate;
    std::string channel_list;

    //setup the program options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("args", po::value<std::string>(&args)->default_value(""), "single uhd device address args")
        ("wire", po::value<std::string>(&wire)->default_value(""), "the over the wire type, sc16, sc8, etc")
        ("secs", po::value<double>(&seconds_in_future)->default_value(1.5), "number of seconds in the future to receive")
        ("nsamps", po::value<size_t>(&total_num_samps)->default_value(10000), "total number of samples to receive")
        ("rate", po::value<double>(&rate)->default_value(100e6/16), "rate of incoming samples")
        ("dilv", "specify to disable inner-loop verbose")
        ("channels", po::value<std::string>(&channel_list)->default_value("0"), "which channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")){
        std::cout << boost::format("UHD RX Timed Samples %s") % desc << std::endl;
        return ~0;
    }

//    bool verbose = vm.count("dilv") == 0;

    //create first usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args_board0 << std::endl;
    uhd::usrp::multi_usrp::sptr usrp0 = uhd::usrp::multi_usrp::make(args_board0);
    std::cout << boost::format("Using Device: %s") % usrp0->get_pp_string() << std::endl;
    //create second usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args_board1 << std::endl;
    uhd::usrp::multi_usrp::sptr usrp1 = uhd::usrp::multi_usrp::make(args_board1);
    std::cout << boost::format("Using Device: %s") % usrp1->get_pp_string() << std::endl;

    //detect which channels to use
    std::vector<size_t> channel_nums;
    channel_nums.push_back(boost::lexical_cast<int>(0));

    //create a receive streamer
    uhd::stream_args_t stream_args("fc32", wire); //complex floats
    stream_args.channels = channel_nums;
    uhd::rx_streamer::sptr rx_stream0 = usrp0->get_rx_stream(stream_args);
    uhd::rx_streamer::sptr rx_stream1 = usrp1->get_rx_stream(stream_args);

    //setup streaming
    std::cout << std::endl;
    std::cout << boost::format(
        "Begin streaming %u samples, %f seconds in the future..."
    ) % total_num_samps % seconds_in_future << std::endl;
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = total_num_samps;
    stream_cmd.stream_now = true;
//    stream_cmd.time_spec = uhd::time_spec_t(seconds_in_future);
    rx_stream0->issue_stream_cmd(stream_cmd);
    rx_stream1->issue_stream_cmd(stream_cmd);


    //allocate buffer to receive with samples
    std::vector<std::complex<float> > buff0(rx_stream0->get_max_num_samps());  // sample are overwritten anyway
    std::vector<std::complex<float> > buff1(rx_stream1->get_max_num_samps());  // sample are overwritten anyway
    std::vector<void *> buffs;
    buffs.push_back(&buff0.front()); //same buffer for each channel
    buffs.push_back(&buff1.front()); //same buffer for each channel

    //meta-data will be filled in by recv()
    uhd::rx_metadata_t md0;
    uhd::rx_metadata_t md1;

    //set the rx sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate/1e6) << std::endl;
    usrp0->set_rx_rate(rate);
    usrp1->set_rx_rate(rate);
    std::cout << boost::format("Actual hepiaB200 RX Rate: %f Msps...") % (usrp0->get_rx_rate()/1e6) << std::endl << std::endl;
    std::cout << boost::format("Actual sinuxB200 RX Rate: %f Msps...") % (usrp1->get_rx_rate()/1e6) << std::endl << std::endl;

    usrp0->set_time_source("external");
    usrp1->set_time_source("external");

    std::cout << boost::format("Setting device timestamp to 100.000...") << std::endl;
    usrp0->set_time_now(uhd::time_spec_t(100.000));
    usrp1->set_time_now(uhd::time_spec_t(100.000));

    size_t num_acc_samps = 0; //number of accumulated samples
//    while(num_acc_samps < total_num_samps){
    while(test_time++ < NB_TESTS) {
        //sleep off if gpsdo detected and time next pps already set
        //boost::this_thread::sleep(boost::posix_time::seconds(1));

        //receive a single packet
        size_t num_rx_samps0 = rx_stream0->recv(
                buffs, buff0.size(), md0, 0.1, true);
        size_t num_rx_samps1 = rx_stream1->recv(
                buffs, buff1.size(), md1, 0.1, true);


        uhd::time_spec_t now0 = usrp0->get_time_now();
        uhd::time_spec_t now1 = usrp1->get_time_now();

        std::cout << boost::format( "Actual hepiaB200 time : %f s")
                     % (now0.get_real_secs()*1e1) << std::endl;

        std::cout << boost::format( "Actual sinuxB200 time : %f s")
                     % (now1.get_real_secs()*1e1) << std::endl;

        std::cout << boost::format( "Difference : %f us" ) % ((now1.get_real_secs() - now0.get_real_secs())*1e6)  << std::endl;
//        std::cout << boost::format( "Actual hepiaB200 time : %u samples, %u full secs, %f frac secs, %ld tics")
//                     % num_rx_samps1 % md1.time_spec.get_full_secs() % md1.time_spec.get_frac_secs() % md1.time_spec.get_tick_count(10000000000000) << std::endl;
//
        num_acc_samps += num_rx_samps0;
    }

    std::cout << boost::format("Last PPS: %f ") %  (usrp1->get_time_last_pps().get_real_secs()*1e6) << std::endl << std::endl;

    //finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

//    usrp0->set_user_register(0x8, 0x32, uhd::usrp::multi_usrp::ALL_MBOARDS);
    usrp0->set_lora_trig(10);

    //set the time at an unknown pps (will throw if no pps)
    std::cout << std::endl << "Attempt to detect the PPS and set the time..." << std::endl << std::endl;
    usrp1->set_time_unknown_pps(uhd::time_spec_t(0.0));
    std::cout << std::endl << "Success!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}
