/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <getopt.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "frame_input.hh"
#include "ivf_reader.hh"
#include "yuv4mpeg.hh"
#include "frame.hh"
#include "player.hh"
#include "vp8_raster.hh"
#include "decoder.hh"
#include "encoder.hh"
#include "macroblock.hh"
#include "ivf_writer.hh"
#include "display.hh"
#include "enc_state_serializer.hh"

using namespace std;

void usage_error( const string & program_name )
{
  cerr << "Usage: " << program_name << " [options] <input>" << endl
       << endl
       << "Options:" << endl
       << " -o <arg>, --output=<arg>              Output file name (default: output.ivf)" << endl
       << " -s <arg>, --ssim=<arg>                SSIM for the output" << endl
       << " -i <arg>, --input-format=<arg>        Input file format" << endl
       << "                                         ivf (default), y4m" << endl
       << " -O <arg>, --output-state=<arg>        Output file name for final" << endl
       << "                                         encoder state (default: none)" << endl
       << " -I <arg>, --input-state=<arg>         Input file name for initial" << endl
       << "                                         encoder state (default: none)" << endl
       << " --two-pass                            Do the second encoding pass" << endl
       << " --y-ac-qi <arg>                       Quantization index for Y" << endl
       << endl;
}

int main( int argc, char *argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc < 2 ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string output_file = "output.ivf";
    string input_format = "ivf";
    string input_state = "";
    string output_state = "";
    double ssim = 0.99;
    bool two_pass = false;

    size_t y_ac_qi = numeric_limits<size_t>::max();

    const option command_line_options[] = {
      { "output",       required_argument, nullptr, 'o' },
      { "ssim",         required_argument, nullptr, 's' },
      { "input-format", required_argument, nullptr, 'i' },
      { "output-state", required_argument, nullptr, 'O' },
      { "input-state",  required_argument, nullptr, 'I' },
      { "two-pass",     no_argument,       nullptr, '2' },
      { "y-ac-qi",      required_argument, nullptr, 'y' },
      { 0, 0, 0, 0 }
    };

    while ( true ) {
      const int opt = getopt_long( argc, argv, "o:s:i:O:I:", command_line_options, nullptr );

      if ( opt == -1 ) {
        break;
      }

      switch ( opt ) {
      case 'o':
        output_file = optarg;
        break;

      case 's':
        ssim = stod( optarg );
        break;

      case 'i':
        input_format = optarg;
        break;

      case 'O':
        output_state = optarg;
        break;

      case 'I':
        input_state = optarg;
        break;

      case '2':
        two_pass = true;
        break;

      case 'y':
        y_ac_qi = stoul( optarg );
        break;

      default:
        throw runtime_error( "getopt_long: unexpected return value." );
      }
    }

    if ( optind >= argc ) {
      usage_error( argv[ 0 ] );
      return EXIT_FAILURE;
    }

    string input_file = argv[ optind ];
    shared_ptr<FrameInput> input_reader;

    if ( input_format == "ivf" ) {
      if ( input_file == "-" ) {
        throw runtime_error( "not supported" );
      }
      else {
        input_reader = make_shared<IVFReader>( input_file );
      }
    }
    else if ( input_format == "y4m" ) {
      if ( input_file == "-" ) {
        input_reader = make_shared<YUV4MPEGReader>( FileDescriptor( STDIN_FILENO ) );
      }
      else {
        input_reader = make_shared<YUV4MPEGReader>( input_file );
      }
    }
    else {
      throw runtime_error( "unsupported input format" );
    }

    Encoder encoder = input_state == ""
      ? Encoder(output_file, input_reader->display_width(), input_reader->display_height(), two_pass)
      : EncoderStateDeserializer::build<Encoder>(input_state, output_file, two_pass);

    Optional<RasterHandle> raster = input_reader->get_next_frame();

    size_t frame_index = 0;

    while ( raster.initialized() ) {
      double result_ssim = encoder.encode( raster.get(), ssim, y_ac_qi );

      cerr << "Frame #" << frame_index++ << ": ssim=" << result_ssim << endl;

      raster = input_reader->get_next_frame();
    }

    if (output_state != "") {
      EncoderStateSerializer odata = {};
      encoder.serialize(odata);
      odata.write(output_state);
    }
  } catch ( const exception &  e ) {
    print_exception( argv[ 0 ], e );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
