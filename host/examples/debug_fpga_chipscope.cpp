/*
 * =====================================================================================
 *
 *       Filename:  debug_fpga_chipscope.cpp
 *
 *    Description:  test read/write access to user setting register (LoRa)
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
    std::string args_board = "name=sinuxB200";
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

    //create first usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args_board << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args_board);
    std::cout << boost::format("Using Device: %s") % usrp->get_pp_string() << std::endl;

    //detect which channels to use
    std::vector<size_t> channel_nums;
    channel_nums.push_back(boost::lexical_cast<int>(0));

    //create a receive streamer
    uhd::stream_args_t stream_args("fc32", wire); //complex floats
    stream_args.channels = channel_nums;
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

    //setup streaming
    std::cout << std::endl;
    std::cout << boost::format(
        "Begin streaming %u samples, %f seconds in the future..."
    ) % total_num_samps % seconds_in_future << std::endl;
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE);
    stream_cmd.num_samps = total_num_samps;
    stream_cmd.stream_now = true;
//    stream_cmd.time_spec = uhd::time_spec_t(seconds_in_future);
    rx_stream->issue_stream_cmd(stream_cmd);


    //allocate buffer to receive with samples
    std::vector<std::complex<float> > buff(rx_stream->get_max_num_samps());  // sample are overwritten anyway
    std::vector<void *> buffs;
    buffs.push_back(&buff.front()); //same buffer for each channel

    //meta-data will be filled in by recv()
    uhd::rx_metadata_t md;

    //set the rx sample rate
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate/1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual hepiaB200 RX Rate: %f Msps...") % (usrp->get_rx_rate()/1e6) << std::endl << std::endl;

    usrp->set_time_source("external");

    std::cout << boost::format("Setting device timestamp to 100.000...") << std::endl;
    usrp->set_time_now(uhd::time_spec_t(100.000));

    size_t num_acc_samps = 0; //number of accumulated samples
//    while(num_acc_samps < total_num_samps){
    while(test_time++ < NB_TESTS) {
        //sleep off if gpsdo detected and time next pps already set
        //boost::this_thread::sleep(boost::posix_time::seconds(1));

        //receive a single packet
        size_t num_rx_samps = rx_stream->recv(
                buffs, buff.size(), md, 0.1, true);

        uhd::time_spec_t now = usrp->get_time_now();

        std::cout << boost::format( "Actual sinuxB200 time : %f s")
                     % (now.get_real_secs()*1e0) << std::endl;

        usrp->set_lora_trig(7);
        usrp->set_lora_trig(1);
        usrp->set_lora_trig(0);

        num_acc_samps += num_rx_samps;
    }

    std::cout << boost::format("Last PPS: %f ") %  (usrp->get_time_last_pps().get_real_secs()*1e6) << std::endl << std::endl;

    //finished
    std::cout << std::endl << "Done!" << std::endl << std::endl;

    //set the time at an unknown pps (will throw if no pps)
    std::cout << std::endl << "Attempt to detect the PPS and set the time..." << std::endl << std::endl;
    usrp->set_time_unknown_pps(uhd::time_spec_t(0.0));
    std::cout << std::endl << "Success!" << std::endl << std::endl;

    return EXIT_SUCCESS;
}
