#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <mecab.h>
#include <phmap.h>

#define CHECK(eval)                                                        \
	if (!eval)                                                             \
	{                                                                      \
		const char* e = tagger ? tagger->what() : MeCab::getTaggerError(); \
		std::cerr << "Exception:" << e << std::endl;                       \
		delete tagger;                                                     \
		return -1;                                                         \
	}

// Note that frustratingly, phmap::flat_hash_map does not support const char* as key
typedef phmap::flat_hash_map<std::string, const char*> DictionaryHashMap;

static DictionaryHashMap dictionary;
static char* rawDictionary = nullptr;
static size_t rawDictionarySize = 0;

void finishAddWordToDictionary(const char* word, size_t wordLength, const char* entry)
{
	// TODO do I need to allocate the key?
	// TODO Memory leak on newKey (iterate over hash map deleting all keys later)
	// +1 for null terminator
	char* newKey = new char[wordLength + 1];
	std::memcpy(newKey, word, wordLength);
	newKey[wordLength] = '\0';
	std::string keyStr(newKey);
	DictionaryHashMap::iterator checkDupeIt = dictionary.find(keyStr);
	if (checkDupeIt != dictionary.end())
	{
		// TODO: Handle
		// std::cout << "Warning: duplicate key '" << keyStr << "' found\n";
	}

	dictionary[keyStr] = entry;

	// TODO Remove
	// Test whether the dictionary is working
	DictionaryHashMap::iterator findIt = dictionary.find(newKey);
	assert(findIt != dictionary.end());
}

void loadDictionary()
{
	// About how many entries there are (lower bound!)
	dictionary.reserve(190000);
	std::cout << "Loading dictionary..." << std::flush;
	std::ifstream inputFile;
	// std::ios::ate so tellg returns the size
	inputFile.open("data/utf8Edict2", std::ios::in | std::ios::binary | std::ios::ate);
	rawDictionarySize = inputFile.tellg();
	rawDictionary = new char[rawDictionarySize];
	inputFile.seekg(0, std::ios::beg);
	inputFile.read(rawDictionary, rawDictionarySize);
	inputFile.close();

	enum class EDict2ReadState
	{
		VersionNumber = 0,
		JapaneseWord,
		Reading,
		EnglishDefinition,
		EntryId
	};

	EDict2ReadState readState = EDict2ReadState::VersionNumber;
	// Multiple ways to say the same "word"
	std::vector<const char*> wordsThisEntry;
	char buffer[1024];
	char* bufferWriteHead = buffer;
	const char* beginningOfLine = nullptr;

#define FINISH_ADD_WORD()                                                             \
	if (bufferWriteHead != buffer)                                                    \
	{                                                                                 \
		finishAddWordToDictionary(buffer, bufferWriteHead - buffer, beginningOfLine); \
		bufferWriteHead = buffer;                                                     \
	}

	for (size_t i = 0; i < rawDictionarySize; ++i)
	{
		if (rawDictionary[i] == '\n')
		{
			// If this is hit, an entry has a format this state machine doesn't understand
			assert(readState == EDict2ReadState::VersionNumber ||
			       readState == EDict2ReadState::EntryId);
			// Reset for the start of next word (words are separated by line)
			beginningOfLine = nullptr;
			readState = EDict2ReadState::JapaneseWord;
			continue;
		}

		switch (readState)
		{
			case EDict2ReadState::VersionNumber:
				// Ignore the whole first line, because it is a different format to report version
				// info
				break;
			case EDict2ReadState::JapaneseWord:
				if (!beginningOfLine)
					beginningOfLine = &rawDictionary[i];

				if (rawDictionary[i] == '/')
				{
					FINISH_ADD_WORD();
					readState = EDict2ReadState::EnglishDefinition;
				}
				else if (rawDictionary[i] == '[')
				{
					FINISH_ADD_WORD();
					readState = EDict2ReadState::Reading;
				}
				else if (rawDictionary[i] == ';')
				{
					// Separate writing of the same word
					FINISH_ADD_WORD();
				}
				else if (rawDictionary[i] == ' ')
				{
					// Ignore all spaces
				}
				else
				{
					*bufferWriteHead = rawDictionary[i];
					++bufferWriteHead;
				}
				break;
			case EDict2ReadState::Reading:
				if (rawDictionary[i] == ']')
				{
					FINISH_ADD_WORD();
					readState = EDict2ReadState::JapaneseWord;
				}
				else if (rawDictionary[i] == ';')
				{
					// Separate reading
					FINISH_ADD_WORD();
				}
				else if (rawDictionary[i] == ' ')
				{
					// Ignore all spaces
				}
				else
				{
					*bufferWriteHead = rawDictionary[i];
					++bufferWriteHead;
				}
				break;
			case EDict2ReadState::EnglishDefinition:
				// Gross. Absorb '/' unless it's clearly the entry ID slash
				if (rawDictionary[i] == '/' && rawDictionary[i + 1] == 'E' &&
				    rawDictionary[i + 2] == 'n' && rawDictionary[i + 3] == 't' &&
				    rawDictionary[i + 4] == 'L')
					readState = EDict2ReadState::EntryId;
				break;
			case EDict2ReadState::EntryId:
				break;
			default:
				break;
		}
	}

#undef FINISH_ADD_WORD

	std::cout << "done.\n" << std::flush;	
}

// bool getDictionaryResults(const char* query, char* outBuffer, size_t outBufferSize)
// {
// 	char* result = strstr(rawDictionary, query);
// 	if (result)
// 	{
// 		// TODO Range check raw Dictionary pointer
// 		for (size_t i = 0; i < outBufferSize; i++)
// 		{
// 			outBuffer[i] = result[i];
// 			if (result[i] == '\n')
// 				break;
// 		}
// 		return true;
// 	}
// 	return false;
// }

bool getDictionaryResults(const char* query, char* outBuffer, size_t outBufferSize)
{
	std::cout << "Find '" << query << "'\n";
	DictionaryHashMap::iterator findIt = dictionary.find(query);
	if (findIt == dictionary.end())
		return false;

	// TODO Range check raw Dictionary pointer
	for (size_t i = 0; i < outBufferSize; i++)
	{
		outBuffer[i] = findIt->second[i];
		if (findIt->second[i] == '\n')
			break;
	}
	return true;
}

void freeDictionary()
{
	delete[] rawDictionary;
}

// Sample of MeCab::Tagger class.
int main(int argc, char** argv)
{
	loadDictionary();
	std::ifstream inputFile;
	// std::ios::ate so tellg returns the size
	inputFile.open("data/Test.org", std::ios::in | std::ios::binary | std::ios::ate);
	size_t size = inputFile.tellg();
	char* memblock = new char[size];
	inputFile.seekg(0, std::ios::beg);
	inputFile.read(memblock, size);
	inputFile.close();

	char* input = memblock;
	// char input[1024] =
	// "\u592a\u90ce\u306f\u6b21\u90ce\u304c\u6301\u3063\u3066\u3044\u308b\u672c\u3092\u82b1\u5b50"
	// "\u306b\u6e21\u3057\u305f\u3002";

	MeCab::Tagger* tagger = MeCab::createTagger("");
	CHECK(tagger);

	// Gets Node object.
	const MeCab::Node* node = tagger->parseToNode(input);
	CHECK(node);
	for (; node; node = node->next)
	{
		if (node->stat == MECAB_BOS_NODE)
			std::cout << "BOS"
			          << "\n";
		else if (node->stat == MECAB_EOS_NODE)
			std::cout << "EOS"
			          << "\n";
		else
		{
			char feature[256] = {0};
			std::memcpy(feature, node->surface,
			            node->length > sizeof(feature) ? sizeof(feature) : node->length);
			feature[node->length] = '\0';
			// for (int i = 0; i < node->length; ++i)
			// feature[i] = *(node->surface + i);

			// char dictionaryResult[512] = {0};
			// if (getDictionaryResults(feature, dictionaryResult, sizeof(dictionaryResult)))
				// std::cout << feature << " " << dictionaryResult << "\n";
			// else
				std::cout << feature << "_\n";
		}

		// std::cout << node->id << ' ';
		// if (node->stat == MECAB_BOS_NODE)
		// 	std::cout << "BOS";
		// else if (node->stat == MECAB_EOS_NODE)
		// 	std::cout << "EOS";
		// else
		// 	std::cout.write(node->surface, node->length);

		std::cout << ' ' << node->feature << ' ' << (int)(node->surface - input) << ' '
		          << (int)(node->surface - input + node->length) << ' ' << node->rcAttr << ' '
		          << node->lcAttr << ' ' << node->posid << ' ' << (int)node->char_type << ' '
		          << (int)node->stat << ' ' << (int)node->isbest << ' ' << node->alpha << ' '
		          << node->beta << ' ' << node->prob << ' ' << node->cost << std::endl;
	}

	delete tagger;

	delete[] memblock;

	freeDictionary();

	return 0;
}
