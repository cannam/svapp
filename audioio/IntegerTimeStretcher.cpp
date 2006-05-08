/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "IntegerTimeStretcher.h"

#include <iostream>
#include <cassert>

//#define DEBUG_INTEGER_TIME_STRETCHER 1

IntegerTimeStretcher::IntegerTimeStretcher(size_t ratio,
					   size_t maxProcessInputBlockSize,
					   size_t inputIncrement,
					   size_t windowSize,
					   WindowType windowType) :
    m_ratio(ratio),
    m_n1(inputIncrement),
    m_n2(m_n1 * ratio),
    m_wlen(std::max(windowSize, m_n2 * 2)),
    m_inbuf(m_wlen),
    m_outbuf(maxProcessInputBlockSize * ratio)
{
    m_window = new Window<double>(windowType, m_wlen),

    m_time = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * m_wlen);
    m_freq = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * m_wlen);
    m_dbuf = (double *)fftw_malloc(sizeof(double) * m_wlen);

    m_plan = fftw_plan_dft_1d(m_wlen, m_time, m_freq, FFTW_FORWARD, FFTW_ESTIMATE);
    m_iplan = fftw_plan_dft_c2r_1d(m_wlen, m_freq, m_dbuf, FFTW_ESTIMATE);

    m_mashbuf = new double[m_wlen];
    for (int i = 0; i < m_wlen; ++i) {
	m_mashbuf[i] = 0.0;
    }
}

IntegerTimeStretcher::~IntegerTimeStretcher()
{
    std::cerr << "IntegerTimeStretcher::~IntegerTimeStretcher" << std::endl;

    fftw_destroy_plan(m_plan);
    fftw_destroy_plan(m_iplan);

    fftw_free(m_time);
    fftw_free(m_freq);
    fftw_free(m_dbuf);

    delete m_window;
    delete m_mashbuf;
}	

size_t
IntegerTimeStretcher::getProcessingLatency() const
{
    return getWindowSize() - getInputIncrement();
}

void
IntegerTimeStretcher::process(double *input, double *output, size_t samples)
{
    // We need to add samples from input to our internal buffer.  When
    // we have m_windowSize samples in the buffer, we can process it,
    // move the samples back by m_n1 and write the output onto our
    // internal output buffer.  If we have (samples * ratio) samples
    // in that, we can write m_n2 of them back to output and return
    // (otherwise we have to write zeroes).

    // When we process, we write m_wlen to our fixed output buffer
    // (m_mashbuf).  We then pull out the first m_n2 samples from that
    // buffer, push them into the output ring buffer, and shift
    // m_mashbuf left by that amount.

    // The processing latency is then m_wlen - m_n2.

    size_t consumed = 0;

#ifdef DEBUG_INTEGER_TIME_STRETCHER
    std::cerr << "IntegerTimeStretcher::process(" << samples << ", consumed = " << consumed << "), writable " << m_inbuf.getWriteSpace() <<", readable "<< m_outbuf.getReadSpace() << std::endl;
#endif

    while (consumed < samples) {

	size_t writable = m_inbuf.getWriteSpace();
	writable = std::min(writable, samples - consumed);

	if (writable == 0) {
	    //!!! then what? I don't think this should happen, but
	    std::cerr << "WARNING: IntegerTimeStretcher::process: writable == 0" << std::endl;
	    break;
	}

#ifdef DEBUG_INTEGER_TIME_STRETCHER
	std::cerr << "writing " << writable << " from index " << consumed << " to inbuf, consumed will be " << consumed + writable << std::endl;
#endif
	m_inbuf.write(input + consumed, writable);
	consumed += writable;

	while (m_inbuf.getReadSpace() >= m_wlen &&
	       m_outbuf.getWriteSpace() >= m_n2) {

	    // We know we have at least m_wlen samples available
	    // in m_inbuf.  We need to peek m_wlen of them for
	    // processing, and then read m_n1 to advance the read
	    // pointer.

	    size_t got = m_inbuf.peek(m_dbuf, m_wlen);
	    assert(got == m_wlen);
		
	    processBlock(m_dbuf, m_mashbuf);

#ifdef DEBUG_INTEGER_TIME_STRETCHER
	    std::cerr << "writing first " << m_n2 << " from mashbuf, skipping " << m_n1 << " on inbuf " << std::endl;
#endif
	    m_inbuf.skip(m_n1);
	    m_outbuf.write(m_mashbuf, m_n2);

	    for (size_t i = 0; i < m_wlen - m_n2; ++i) {
		m_mashbuf[i] = m_mashbuf[i + m_n2];
	    }
	    for (size_t i = m_wlen - m_n2; i < m_wlen; ++i) {
		m_mashbuf[i] = 0.0f;
	    }
	}

//	std::cerr << "WARNING: IntegerTimeStretcher::process: writespace not enough for output increment (" << m_outbuf.getWriteSpace() << " < " << m_n2 << ")" << std::endl;
//	}

#ifdef DEBUG_INTEGER_TIME_STRETCHER
	std::cerr << "loop ended: inbuf read space " << m_inbuf.getReadSpace() << ", outbuf write space " << m_outbuf.getWriteSpace() << std::endl;
#endif
    }

    if (m_outbuf.getReadSpace() < samples * m_ratio) {
	std::cerr << "WARNING: IntegerTimeStretcher::process: not enough data (yet?) (" << m_outbuf.getReadSpace() << " < " << (samples * m_ratio) << ")" << std::endl;
	size_t fill = samples * m_ratio - m_outbuf.getReadSpace();
	for (size_t i = 0; i < fill; ++i) {
	    output[i] = 0.0;
	}
	m_outbuf.read(output + fill, m_outbuf.getReadSpace());
    } else {
#ifdef DEBUG_INTEGER_TIME_STRETCHER
	std::cerr << "enough data - writing " << samples * m_ratio << " from outbuf" << std::endl;
#endif
	m_outbuf.read(output, samples * m_ratio);
    }

#ifdef DEBUG_INTEGER_TIME_STRETCHER
    std::cerr << "IntegerTimeStretcher::process returning" << std::endl;
#endif
}

void
IntegerTimeStretcher::processBlock(double *buf, double *out)
{
    size_t i;

    // buf contains m_wlen samples; out contains enough space for
    // m_wlen * ratio samples (we mix into out, rather than replacing)

#ifdef DEBUG_INTEGER_TIME_STRETCHER
    std::cerr << "IntegerTimeStretcher::processBlock" << std::endl;
#endif

    m_window->cut(buf);

    for (i = 0; i < m_wlen/2; ++i) {
	double temp = buf[i];
	buf[i] = buf[i + m_wlen/2];
	buf[i + m_wlen/2] = temp;
    }
    
    for (i = 0; i < m_wlen; ++i) {
	m_time[i][0] = buf[i];
	m_time[i][1] = 0.0;
    }

    fftw_execute(m_plan); // m_time -> m_freq

    for (i = 0; i < m_wlen; ++i) {
	
	double mag = sqrt(m_freq[i][0] * m_freq[i][0] +
			  m_freq[i][1] * m_freq[i][1]);
		
	double phase = atan2(m_freq[i][1], m_freq[i][0]);
	
	phase = phase * m_ratio;
	
	double real = mag * cos(phase);
	double imag = mag * sin(phase);
	m_freq[i][0] = real;
	m_freq[i][1] = imag;
    }
    
    fftw_execute(m_iplan); // m_freq -> in, inverse fft
    
    for (i = 0; i < m_wlen/2; ++i) {
	double temp = buf[i] / m_wlen;
	buf[i] = buf[i + m_wlen/2] / m_wlen;
	buf[i + m_wlen/2] = temp;
    }
    
    m_window->cut(buf);
    
    int div = m_wlen / m_n2;
    if (div > 1) div /= 2;
    for (i = 0; i < m_wlen; ++i) {
	buf[i] /= div;
    }

    for (i = 0; i < m_wlen; ++i) {
	out[i] += buf[i];
    }
}