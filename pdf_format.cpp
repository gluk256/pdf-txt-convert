// Converting PDF file into plain text format is tricky.
// Simple saving the document as plain text yields 
// weird-looking file with a lot of garbage inside. 
// This project is designed to solve the problem.
// Performance of this program is not critical.
// The text is expected to contain only letters of English alphabeth.

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <locale>

using namespace std;

class Converter
{
public:
	std::locale loc;
	string zero;

	ifstream fsrc;
	ofstream fdst;

	list<string> m_dst;
	vector<string> m_src;

	string dst_pending;
	string cur_headline;

	unsigned m_average_strlen;
	unsigned m_upper_cnt;
	unsigned m_word_cnt;
	unsigned m_debug;

	bool garbage, headline, prev_headline, all_upper, contains_lower, separate_line, new_line;

	static void wait();
	static void process_error(const char* msg);

	Converter() : m_average_strlen(0), m_debug(0) {}
	void init(int argn, char** argv);
	void read_file(const char* fname);
	void convert();	
	void test();
	void calculate_average_str_len();
	void open_dst_file(const char* fname);
	bool check_garbage(const string& s);
	bool check_headline(unsigned i);
	bool check_all_upper(const string& s);
	bool check_url(const string& s);
	bool check_contains_lower(const string& s);
	bool check_contains_alphanum(const string& s);	
	bool check_new_line(unsigned i);		
	bool check_page_markup(const string& s);
	bool check_all_words_start_with_capital(const string& s);
	bool check_separate_line(unsigned i);
	char get_last_char(const string& s);
	unsigned first_word_size(const string& s);
	unsigned last_word_size(const string& s);
	void count_words(const string& s);
	bool is_equal_nocase(const string& s1, const string& s2);
	bool is_end_of_sentence(char c);
	void substitute_special_symbols(string& s);
	void replace_sequence(string& s, const string& sequence, const string& substitute);
	string& get_prev_str(unsigned i);
	string& get_next_str(unsigned i);
	void write();
	void analize(unsigned i);
	void process(unsigned i);
	void flush_pending();
	void remove_odd_special_symbols(string& s);
	bool is_glyph(char c) { return c < 0 || c >= 32; }
};

void Converter::write()
{
	for (list<string>::const_iterator i = m_dst.begin(); i != m_dst.end(); ++i)
		fdst << i->c_str() << endl;
}

string& Converter::get_next_str(unsigned i)
{
	if (++i >= m_src.size())
		return zero;
	else
		return m_src[i];
}

string& Converter::get_prev_str(unsigned i)
{
	if (0 == i || i > m_src.size())
		return zero;

	string& s = m_src[i-1];
	return s;
}

void Converter::replace_sequence(string& s, const string& sequence, const string& substitute)
{
	for (unsigned i = s.find(sequence, 0); string::npos != i; i = s.find(sequence, i)) 
		s.replace(i, sequence.size(), substitute);
}

void Converter::substitute_special_symbols(string& s)
{
	replace_sequence(s, "\xE2\x80\x94", " -- ");
	replace_sequence(s, "\xE2\x80\x99", "'");
	replace_sequence(s, "\xE2\x80\x9C", "\"");
	replace_sequence(s, "\xE2\x80\x9D", "\"");
	replace_sequence(s, "\xE2\x80\xA2", "* ");
	replace_sequence(s, "\xC2\xA9", "(C)");
	replace_sequence(s, "\xC3\xA9", "e");

	remove_odd_special_symbols(s);
}

void Converter::remove_odd_special_symbols(string& s)
{
	char prev = 'x', next = 'x', c = 0;
	string::iterator j;
	string::iterator i = s.begin();

	while (i != s.end()) {
		c = *i;
		j = i;
		if (++j != s.end())
			next = *j;
		else
			next = 'x';

		if (!is_glyph(c) && is_glyph(next) && is_glyph(prev))
			i = s.erase(i);
		else
			++i;

		prev = c;
	}
}

bool Converter::is_end_of_sentence(char c)
{
	bool res = ('.' == c || '!' == c || '?' == c || '"' == c);
	return res;
}

bool Converter::is_equal_nocase(const string& s1, const string& s2)
{
	if (s1.size() != s2.size())
		return false;

	for (unsigned i = 0; i < s1.size(); ++i)
		if (tolower(s1[i]) != tolower(s2[i]))
			return false;

	return true;
}

void Converter::count_words(const string& s)
{
	unsigned i = 0, cnt = 0;

	while (i < s.size() && ' ' == s[i])
		++i;
	
	for (;;)
	{		
		i = s.find(' ', i);
		if (string::npos == i)
			break;
		else
			++cnt, ++i;
	}

	i = s.size();
	while (i > 0) {
		if (' ' == s[--i])
			--cnt;
		else
			break;
	}

	m_word_cnt = ++cnt;
}

bool Converter::check_url(const string& s)
{
	const string must = "http://";
	unsigned pos = s.find(must);
	if (string::npos == pos)
		return false;
	
	unsigned pos2 = s.find(' ', pos);
	string x = s;
	x.erase(pos, pos2);
	if (!check_contains_lower(x) && !m_upper_cnt)
		return true;

	return false;
}

bool Converter::check_all_upper(const string& s)
{
	int upp = m_upper_cnt = 0;

	for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
		if (std::islower(*i, loc))
			return false;
		else if (std::isupper(*i, loc))
			++upp;
	}

	m_upper_cnt = upp;
	return true;
}

bool Converter::check_contains_lower(const string& s)
{
	int low = 0;

	for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
		if (std::islower(*i, loc))
			++low;
	}

	return low > 0;
}

bool Converter::check_contains_alphanum(const string& s)
{
	int cnt = 0;

	for (string::const_iterator i = s.begin(); i != s.end(); ++i) {
		if (std::isgraph(*i, loc))
			++cnt;		
	}

	return cnt > 0;
}

bool Converter::check_page_markup(const string& s)
{
	if (!contains_lower && all_upper && m_upper_cnt > (s.size()/3))
		return true;
	else
		return false;
}

bool Converter::check_garbage(const string& s)
{
	if (m_dst.empty() && s.size() < 6 && !check_contains_alphanum(s))
		return true;

	if (check_page_markup(s))
		return true;

	if (check_url(s))
		return true;

	if (!check_contains_alphanum(s))
		return true;

	return false;
}

void Converter::process_error(const char* msg)
{
	ofstream logg("verter.log");
	logg << msg << endl;
	cout << msg << endl;
}

void Converter::wait()
{
	#ifdef _DEBUG
	char c;
	cin >> c;
	#endif
}

void Converter::read_file(const char* fname)
{
	const unsigned int SZ = 8196;
	char buf[SZ];
	string s;

	ifstream f(fname);
	if (!f.good()) {
		string s = "Can not open file ";
		s += fname;
		throw std::exception(s.c_str());
	}

	while (f.good()) {
		f.getline(buf, SZ);
		s = buf;
		substitute_special_symbols(s);
		m_src.push_back(s);
	}
}

void Converter::open_dst_file(const char* src)
{
	string s = src;
	unsigned pos = s.find('.');
	if (string::npos == pos)
		s += ".txt";
	else
		s.insert(pos, "~");

	fdst.open(s.c_str());
	if (!fdst.good()) {
		string msg = "can not open destination file ";
		msg += s;
		throw std::exception(s.c_str());
	}
}

void Converter::test()
{
	for (vector<string>::iterator i = m_src.begin(); i != m_src.end(); ++i)
		cout << i->c_str() << endl;
}

void Converter::calculate_average_str_len()
{
	unsigned avg = 0, sum = 0, i = 0, cnt = 0;
	unsigned sz = m_src.size() / 3;

	for (i = sz; i < sz + sz; ++i) {
		if (!m_src[i].empty()) {
			sum += m_src[i].size();
			++cnt;
		}
	}

	if (cnt)
		avg = sum / cnt;

	cnt = sum = 0;

	for (i = sz; i < sz + sz; ++i) {
		if (m_src[i].size() > avg) {
			sum += m_src[i].size();
			++cnt;
		}
	}

	if (cnt)
		avg = sum / cnt;

	++m_debug;
	m_average_strlen = avg;
}

void Converter::init(int argn, char** argv)
{
	if (argn < 2) 
		throw std::exception("wrong number of parameters: too few");

	read_file(argv[1]);
	open_dst_file(argv[1]);
	calculate_average_str_len();
}

bool Converter::check_separate_line(unsigned i)
{
	if (i >= m_src.size())
		return false;
	
	string& s = m_src[i];

	char last = get_last_char(s);
	char first = s[0];

	if (first >= '0' && first <= '9' && last >= '0' && last <= '9')
		return true;

	if (i > 0 && is_glyph(first)) {
		char c_cur = get_last_char(s);
		char c_prev = get_last_char(m_src[i-1]);
		if (is_end_of_sentence(c_cur) && !is_end_of_sentence(c_prev) && std::islower(first, loc))
			return false;
	}

	unsigned possible_size_prev = get_prev_str(i).size() + first_word_size(s);
	unsigned fws = first_word_size(get_next_str(i));
	unsigned lws = last_word_size(get_prev_str(i));
	unsigned possible_size_cur = fws + s.size() + lws;
	if (possible_size_cur < (m_average_strlen * 2 / 3) && possible_size_prev < m_average_strlen)
		return true;

	return false;
}

bool Converter::check_new_line(unsigned i)
{
	if (i >= m_src.size())
		return false;
	
	string& s = m_src[i];

	if (get_prev_str(i).empty() || prev_headline)
		return true;

	if ('*' == s[0])
		return true;

	char c = get_last_char(get_prev_str(i));
	if (!is_end_of_sentence(c))
		return false;

	if (std::islower(s[0], loc))
		return false;

	unsigned possible_size_prev = get_prev_str(i).size() + first_word_size(s);
	if (possible_size_prev < m_average_strlen)
		return true;

	return false;
}

bool Converter::check_headline(unsigned i)
{
	if (i >= m_src.size() || m_src[i].empty())
		return false;
	
	string& s = m_src[i];

	if (!check_all_words_start_with_capital(s))
		return false;

	char last = get_last_char(s);
	if (last < 32)
		return false;

	if ('.' == last)
		return false;

	if (is_equal_nocase(s, "Contents"))
		return true;

	if (isdigit(last) && is_equal_nocase(cur_headline, "Contents"))
		return false;

	if ('*' == s[0])
		return false;

	return true;
}

char Converter::get_last_char(const string& s)
{
	unsigned x = s.size() - 1;
	if (x < 0)
		return 0;
	else
		return s[x];
}

bool Converter::check_all_words_start_with_capital(const string& s)
{
	if (std::islower(s[0], loc))
		return false;

	unsigned i = 1;
	
	for (;;)
	{
		i = s.find(' ', i);
		if (string::npos == i)
			break;
		
		if (++i >= s.size()-1)
			break;

		if ('"' == s[i])
			if (++i >= s.size()-1)
				break;

		if (std::islower(s[i], loc))
			return false;
	}
	
	return true;
}

unsigned Converter::first_word_size(const string& s)
{
	unsigned pos = s.find(' ');
	if (string::npos == pos)
		return s.size();
	else
		return pos;
}

unsigned Converter::last_word_size(const string& s)
{
	unsigned pos = s.find_last_of(' ');
	if (string::npos == pos)
		return s.size();
	else
		return s.size() - pos;
}

void Converter::flush_pending()
{
	if (!dst_pending.empty()) {
		m_dst.push_back(dst_pending);
		dst_pending.clear();
	}	
}

void Converter::process(unsigned i)
{
	string& s = m_src[i];
	if (s.empty() || garbage) 
		return;

	if (headline) {
		flush_pending();

		if (prev_headline && !m_dst.empty() && m_dst.back().empty())
			m_dst.pop_back();
		else if (!m_dst.empty() && !m_dst.back().empty())
			m_dst.push_back(zero);

		m_dst.push_back(s);
		m_dst.push_back(zero);
	}
	else if (separate_line) {
		flush_pending();
		m_dst.push_back(s);
	}
	else if (new_line) {
		flush_pending();
		dst_pending += s;
	}
	else
	{
		if (!dst_pending.empty())
			dst_pending += ' ';

		dst_pending += s;
	}
}

void Converter::analize(unsigned i)
{
	string& s = m_src[i];
	if (s.empty()) 
		return;

	if (!s.compare("ee)a-dtoadya:y\":WYaEsSt!oday"))
		++m_debug;

	all_upper = check_all_upper(s);
	contains_lower = check_contains_lower(s);
	count_words(s);

	garbage = check_garbage(s);
	if (garbage)
		return;

	headline = check_headline(i);
	if (headline) {
		cur_headline = s;
		return;
	}

	separate_line = check_separate_line(i);
	new_line = check_new_line(i);
}

void Converter::convert()
{
	prev_headline = true;

	for (unsigned i = 0; i < m_src.size(); ++i) {
		headline = all_upper = contains_lower = garbage = separate_line = new_line = false;
		analize(i);
		process(i);
		prev_headline = headline;
	}
}

int main(int argn, char** argv)
{
	try {
		Converter x;
		x.init(argn, argv);
		x.convert();
		x.write();
		cout << "Completed successfully" << endl;
	}
	catch(std::exception& ex) {
		Converter::process_error(ex.what());
	}

	Converter::wait();
	return 0;
}
