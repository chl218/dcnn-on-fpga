#include "stream_deconv.h"

//==============================================================================
// Fixed point arithmetics
//==============================================================================
inline d_int _max (p_bool x, d_int y) {
	if(x > y) return (d_int)x;
	else return y;
}

inline p_int _min (p_int x, p_int y) {
	if (x < y) return x;
	else return y;
}

inline d_int _multiply (d_int a, d_int b) {
	d_long c=0;
	c = (d_long)a * (d_long)b;
	c >>= SHIFT_AMOUNT;
	return (d_int)c;
}

inline d_int _divide (d_int a, d_int b ) {
	d_long c=0;
	c = (d_long)a << SHIFT_AMOUNT;
	c /= b;
	return (d_int)c;
}

//=============================================================================
// Layer 1
//
// 		layer_params hidden1 = {1,1,10, 4,4,32, 4,2,0, 1,0};
//
//=============================================================================
inline void _extend_stream_1(hls::stream<d_int> &stream_i, hls::stream<d_int> &stream_o) {
	d_int buffer[32];
#pragma HLS ARRAY_PARTITION variable=buffer complete dim=1

	L_RD: for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
		buffer[oc] = stream_i.read();
	}

	L_REP: for(d_int rep_amt = 0; rep_amt < 16; rep_amt++) {
#pragma HLS PIPELINE II=1
		L_WT: for(d_int oc = 0; oc < 32; oc++) {
			stream_o.write(buffer[oc]);
		}
	}
}

void stream_deconv_1(hls::stream<d_int> &stream_i,
					 hls::stream<d_int> &kernel,
					 hls::stream<d_int> &bias,
					 hls::stream<d_int> &mean,
					 hls::stream<d_int> &std,
					 hls::stream<d_int> &stream_o){

	d_int in_buf[10];
#pragma HLS ARRAY_PARTITION variable=in_buf complete dim=1

	// Buffer Input
	L_Buf: for(d_int ic = 0; ic < 10; ic++) {
#pragma HLS PIPELINE II=1
		in_buf[ic] = stream_i.read();
	}

	// Pad Zeros
	for(d_int ow = 0; ow < 9; ow++) {
#pragma HLS PIPELINE II=1
		for(d_int oc = 0; oc < 32; oc++) {
			stream_o.write(0);
		}
	}

	L_OH: for(d_int oh = 0; oh < 4; oh++) {					// Height
		// Pad Zeros
		for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
			stream_o.write(0);
		}

		L_OW: for(d_int ow = 0; ow < 4; ow++) {				// Width
			L_OC: for(d_int oc = 0; oc < 32; oc++) {		// Out-Channels
#pragma HLS PIPELINE II=1
				d_temp acc = bias.read();
				L_IC: for(d_int ic = 0; ic < 10; ic++) {	// In-Channels

					acc += _multiply(in_buf[ic],  kernel.read());
					if(acc > UPPER_BOUND)      acc = UPPER_BOUND;
					else if(acc < LOWER_BOUND) acc = LOWER_BOUND;

				}
				stream_o.write(_max(0, _divide(acc - mean.read(), std.read())));
			} // end Out-Channels

			// Pad Zeros
			for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
				stream_o.write(0);
			}
		} // end Width

		// Pad Zeros
		for(d_int ow = 0; ow < 9; ow++) {
#pragma HLS PIPELINE II=1
			for(d_int oc = 0; oc < 32; oc++) {
				stream_o.write(0);
			}
		}

	} // end L_Oh
}


//=============================================================================
// Layer 2
//
// 		layer_params hidden2 = {4,4,32, 12,12,32, 6,2,0, 1,0};
//
//=============================================================================

static d_int layer2_kernel[6][6][32][32];
inline void _wt_kernel_2(layer_params param, hls::stream<d_int> &kernel_i) {
	L_KH: for(d_int kh = 0; kh < 6; kh++) {
		L_KW: for(d_int kw = 0; kw < 6; kw++) {
			L_OC: for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
				L_IC: for(d_int ic = 0; ic < 32; ic++) {
					layer2_kernel[kh][kw][oc][ic] = kernel_i.read();
				}
			}
		}
	}
}

inline void _extend_stream_2(hls::stream<d_int> &stream_i, hls::stream<d_int> &stream_o) {
	d_int buffer[32];
#pragma HLS ARRAY_PARTITION variable=buffer complete dim=1


	for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
		buffer[oc] = stream_i.read();
	}

	for(d_int rep_amt = 0; rep_amt < 144; rep_amt++) {
#pragma HLS PIPELINE II=1
		for(d_int oc = 0; oc < 32; oc++) {
			stream_o.write(buffer[oc]);
		}
	}
}

void stream_deconv_2(//layer_params param,
					hls::stream<d_int> &stream_i,
					hls::stream<d_int> &bias,
					hls::stream<d_int> &mean,
					hls::stream<d_int> &std,
					hls::stream<d_int> &stream_o) {

	d_int layer2_bias[32];
#pragma HLS ARRAY_PARTITION variable=layer2_bias complete dim=1

	d_temp layer2_matrix[14][14][32];
#pragma HLS ARRAY_PARTITION variable=layer2_matrix complete factor=4 dim=3

	// Initialize bias
	for(d_int oc = 0; oc < 32; oc++) {
		d_int b = bias.read();
		for(d_int oh = 0; oh < 14; oh++) {
			for(d_int ow = 0; ow < 14; ow++) {
#pragma HLS PIPELINE II=1
				layer2_matrix[oh][ow][oc] = b;
			}
		}
	}


	// For each input row and column
	L2_IH: for(d_int ih = 0; ih < 9; ih++) {
		L2_IW: for(d_int iw = 0; iw < 9; iw++) {

			// save (i, j) depth
			d_int in_buf[32];
#pragma HLS ARRAY_PARTITION variable=in_buf complete dim=1
			L2_BUF_INSTREAM: for(d_int ic = 0; ic < 32; ic++) {
#pragma HLS PIPELINE II=1
				in_buf[ic] = stream_i.read();
			}

			L2_OC: for(d_int oc = 0; oc < 32; oc++) { 			 // Output channel
				L2_KH: for(d_int kh = 0; kh < 6; kh++) { 		 // Kernel height
					L2_KW: for(d_int kw = 0; kw < 6; kw++) { 	 // Kernel width
#pragma HLS PIPELINE II=1
						L2_IC: for(d_int ic = 0; ic < 32; ic++) { // Input channel
							layer2_matrix[ih+kh][iw+kw][oc] += _multiply(in_buf[ic], layer2_kernel[kh][kw][oc][ic]);
							if(layer2_matrix[ih+kh][iw+kw][oc]> UPPER_BOUND)       layer2_matrix[ih+kh][iw+kw][oc] = UPPER_BOUND;
							else if(layer2_matrix[ih+kh][iw+kw][oc] < LOWER_BOUND) layer2_matrix[ih+kh][iw+kw][oc] = LOWER_BOUND;
						}
					}
				}
			} // end deconv
		} // end width
	} // end height

	// Stream out layer matrix, displace first and last row, and fist and last column

	for(d_int ow = 0; ow < 25; ow++) {
		for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
			stream_o.write(0);
		}
	}

	for(d_int oh = 1; oh < 13; oh++) {
		for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
			stream_o.write(0);
		}

		for(d_int ow = 1; ow < 13; ow++) {
			for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
				// Normalization
				d_int norm = _divide(layer2_matrix[oh][ow][oc] - mean.read(), std.read());
				// ReLu
				stream_o.write(_max(0, norm));
			}

			for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
				stream_o.write(0);
			}
		}

		for(d_int ow = 0; ow < 25; ow++) {
			for(d_int oc = 0; oc < 32; oc++) {
#pragma HLS PIPELINE II=1
				stream_o.write(0);
			}
		}
	}
}

//=============================================================================
// Layer 3
//
// 		layer_params hidden2 = {12,12,32, 28,28, 1, 6,2,0, 0,1};
//
//=============================================================================

static d_int layer3_kernel[6][6][32];
inline void _wt_kernel_3(layer_params param, hls::stream<d_int> &kernel_i) {
	L_KH: for(d_int kh = 0; kh < L3KH; kh++) {
		L_KW: for(d_int kw = 0; kw < L3KW; kw++) {
#pragma HLS PIPELINE II=1
			L_IC: for(d_int ic = 0; ic < L3KIC; ic++) {
				layer3_kernel[kh][kw][ic] = kernel_i.read();
			}
		}
	}
}

void stream_deconv_3(//layer_params param,
					 hls::stream<d_int> &stream_i,
					 d_int bias,
					 hls::stream<d_int> &stream_o) {

	d_temp layer3_matrix[30][30];

	for(d_int oh = 0; oh < 30; oh++) {
		for(d_int ow = 0; ow < 30; ow++) {
#pragma HLS PIPELINE II=1
			layer3_matrix[oh][ow] = bias;
		}
	}


	L2_IH: for(d_int ih = 0; ih < 25; ih++) {
		L2_IW: for(d_int iw = 0; iw < 25; iw++) {

			d_int in_buf[32];
#pragma HLS ARRAY_PARTITION variable=in_buf complete dim=1
			for(d_int ic = 0; ic < 32; ic++) {
				in_buf[ic] = stream_i.read();
			}

			L2_KH: for(d_int kh = 0; kh < 6; kh++) { 		 	// Height
				L2_KW: for(d_int kw = 0; kw < 6; kw++) { 	 	// Width
#pragma HLS PIPELINE II=1
					L2_IC: for(d_int ic = 0; ic < 32; ic++) { 	// Input Channel
						layer3_matrix[ih+kh][iw+kw] += _multiply(in_buf[ic], layer3_kernel[kh][kw][ic]);
						if(layer3_matrix[ih+kh][iw+kw]> UPPER_BOUND)       layer3_matrix[ih+kh][iw+kw] = UPPER_BOUND;
						else if(layer3_matrix[ih+kh][iw+kw] < LOWER_BOUND) layer3_matrix[ih+kh][iw+kw] = LOWER_BOUND;

					}
				}
			}
		} // end width
	} // end height


	for(d_int oh = 1; oh < 29; oh++) {
		for(d_int ow = 1; ow < 29; ow++) {
#pragma HLS PIPELINE II=1
			stream_o.write(layer3_matrix[oh][ow]);
		}
	}
}


//=============================================================================
// Top-Level
//=============================================================================
void deconv(layer_params param[3],
			hls::stream<d_int> &stream_i,
			hls::stream<d_int> (&kernel)[3],
			hls::stream<d_int> (&bias)[3],
			hls::stream<d_int> (&mean)[2],
			hls::stream<d_int> (&std)[2],
			hls::stream<d_int> &stream_o) {
#pragma HLS DATAFLOW

	hls::stream<d_int> stream_res[2];

	layer_params param0 = param[0];
	layer_params param1 = param[1];
	layer_params param2 = param[2];

	// pre-process
	// =========================================================================
	hls::stream<d_int> bias0, mean0, std0;
	_extend_stream_1(bias[0], bias0);
	_extend_stream_1(mean[0], mean0);
	_extend_stream_1(std[0],  std0);

	hls::stream<d_int> mean1, std1;
	_wt_kernel_2(param1, kernel[1]);
	_extend_stream_2(mean[1], mean1);
	_extend_stream_2(std[1],  std1);

	_wt_kernel_3(param2, kernel[2]);

	// deconvolution
	// =========================================================================
	stream_deconv_1(stream_i, kernel[0], bias0, mean0, std0, stream_res[0]);
	stream_deconv_2(stream_res[0], bias[1], mean1, std1, stream_res[1]);
	stream_deconv_3(stream_res[1], bias[2].read(), stream_o);
}

