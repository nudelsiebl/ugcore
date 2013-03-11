#include "common/util/base64_file_writer.h"

// for base64 encoding with boost
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

// debug includes!!
#include "common/profiler/profiler.h"
#include "common/error.h"
#include "common/assert.h"

using namespace std;

/**
 * \brief This is the actual encoder using Boost iterators
 */
typedef boost::archive::iterators::base64_from_binary<
		// convert binary values to base64 characters
			boost::archive::iterators::transform_width<
		// retrieve 6 bit integers from a sequence of 8 bit bytes
			const char *, 6, 8>
		// compose all the above operations in to a new iterator
		> base64_text;

//ostream& operator << (ostream& os, ug::Base64FileWriter::fmtflag f) {
//	switch(f) {
//	case ug::Base64FileWriter::normal:
//		os << "normal";
//		break;
//	case ug::Base64FileWriter::base64_ascii:
//		os << "base64_ascii";
//		break;
//	case ug::Base64FileWriter::base64_binary:
//		os << "base64_binary";
//		break;
//	}
//	return os;
//}

namespace ug {

////////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS

Base64FileWriter::fmtflag Base64FileWriter::format() const {
	return m_currFormat;
}

Base64FileWriter& Base64FileWriter::operator<<(const fmtflag format)
{
	PROFILE_FUNC();

	// forceful flushing of encoder's internal input buffer is necessary
	// if we are switching formats.
	if (format != m_currFormat && format == normal) {
		if (m_numBytesWritten > 0) {
			flushInputBuffer(true);
		}
	}
	m_currFormat = format;
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(int value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(char value) {
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(const char* value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(const string& value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(float value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(double value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(long value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter& Base64FileWriter::operator<<(size_t value)
{
	dispatch(value);
	return *this;
}

Base64FileWriter::Base64FileWriter() :
	m_currFormat(base64_ascii),
	m_inBuffer(ios_base::binary | ios_base::out | ios_base::in),
	m_lastInputByteSize(0),
	m_numBytesWritten(0),
	m_numBytesInBlock(0)
{}

Base64FileWriter::Base64FileWriter(const char* filename,
		const ios_base::openmode mode) :
	m_currFormat(base64_ascii),
	m_inBuffer(ios_base::binary | ios_base::out | ios_base::in),
	m_lastInputByteSize(0),
	m_numBytesWritten(0),
	m_numBytesInBlock(0)
{
	PROFILE_FUNC();

	open(filename, mode);
}

Base64FileWriter::~Base64FileWriter()
{
	flushInputBuffer(true);
	m_fStream.close();
}

void Base64FileWriter::open(const char *filename,
								const ios_base::openmode mode)
{
	m_fStream.open(filename, mode);
	if (!m_fStream.is_open()) {
		UG_THROW( "Could not open output file: " << filename);
	} else if (!m_fStream.good()) {
		UG_THROW( "Can not write to output file: " << filename);
	}
}

////////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS

// this function performs conversion to plain const char* and stores it in
// m_inBuffer, either raw for binary mode or as string conversion in base64_ascii.
template <typename T>
void Base64FileWriter::dispatch(const T& value)
{
	PROFILE_FUNC();
	assertFileOpen();

	switch ( m_currFormat ) {
		case base64_ascii:
			// create string representation of value and store it in buffer
			m_inBuffer << value;
			// written bytes is equivalent to current write pointer pos
			m_numBytesWritten = m_inBuffer.tellp();
			m_lastInputByteSize = sizeof(char);
			// check if a buffer flush is needed
			flushInputBuffer();
			break;
		case base64_binary: {
			// write the value in binary mode to the input buffer
			UG_ASSERT(m_inBuffer.good(), "can not write to buffer")
			m_inBuffer.write(reinterpret_cast<const char*>(&value), sizeof(T));
			UG_ASSERT(m_inBuffer.good(), "write failed")

			m_numBytesWritten += sizeof(T);
			m_lastInputByteSize = sizeof(T);
			// check if a buffer flush is needed
			flushInputBuffer();
			break;
		}
		case normal:
			// nothing to do here, almost
			m_fStream << value;
			break;
	}
}

inline void Base64FileWriter::assertFileOpen()
{
	if (m_fStream.bad() || !m_fStream.is_open()) {
		UG_THROW( "File stream is not open." );
	}
}

void Base64FileWriter::flushInputBuffer(bool force)
{
	PROFILE_FUNC();

	size_t buff_len = 0;
	// amount of elements to flush at once
	const uint elements_to_flush = 32;
	// in case of normal format, no input size is known, so this evals to zero.
	const uint bytes_to_flush = 3 * m_lastInputByteSize * elements_to_flush;

	// if force, flush all bytes in input stream
	if (force) {
		buff_len = m_numBytesWritten;
	} else if ( bytes_to_flush <= m_numBytesWritten) {
		buff_len = bytes_to_flush;
	}

	if (buff_len == 0) {
		if(force) { // empty buffer and force => we're finished
			addPadding(m_numBytesInBlock);
		}
		return;
	}

	m_tmpBuff.clear();
	m_tmpBuff.reserve(buff_len);
	char* buff = &m_tmpBuff[0];

	if (!m_inBuffer.good()) {
		UG_THROW("input buffer not good before read");
	}

	// read the buffer
	m_inBuffer.read(buff, buff_len);

	if (!m_inBuffer.good()) {
		UG_THROW("failed to read from input buffer");
	}

	// encode buff in base64
	copy(base64_text(buff), base64_text(buff + buff_len),
			boost::archive::iterators::ostream_iterator<char>(m_fStream));

	size_t rest_len = m_numBytesWritten - buff_len;

	if (rest_len > 0) {
		m_tmpBuff.clear();
		m_tmpBuff.reserve(rest_len);
		char* rest = &m_tmpBuff[0];
		// read the rest
		m_inBuffer.read(rest, rest_len);
		if (!m_inBuffer.good()) {
			UG_THROW("failed to read from input buffer");
		}

		// reset the internal string
		m_inBuffer.str("");
		// set write pos to beginning
		m_inBuffer.seekp(0, ios_base::beg);

		// and write the rest
		m_inBuffer.write(rest, rest_len);
		if (!m_inBuffer.good()) {
			UG_THROW("failed to write from input buffer");
		}
	} else {
		// reset buffer
		m_inBuffer.str("");
	}

	// set read and write position to beginning
	m_inBuffer.seekp(0, ios_base::beg);
	m_inBuffer.seekg(0, ios_base::beg);

	// increment bytes in this block by buffer already encoded
	m_numBytesInBlock += buff_len;

	if (force) {
		addPadding(m_numBytesInBlock);

		// resetting num bytes written and bytes in block
		m_numBytesWritten = 0;
		m_numBytesInBlock = 0;
	} else {
		// update amount of bytes in input buffer
		m_numBytesWritten -= buff_len;
	}
}

void Base64FileWriter::addPadding(size_t blockSize)
{
	PROFILE_FUNC();
	// perform padding if last triplet has only 1 or 2 bytes
	uint writePaddChars = (3 - blockSize % 3) % 3;
	for (uint i = 0; i < writePaddChars; i++)
		m_fStream << '=';
}

void Base64FileWriter::close()
{
	PROFILE_FUNC();

	// make sure all remaining content of the input buffer is encoded and flushed
	flushInputBuffer(true);

	// only when this is done, close the file stream
	m_fStream.close();
	UG_ASSERT(m_fStream.good(), "could not close output file.");
}

}	// namespace ug
