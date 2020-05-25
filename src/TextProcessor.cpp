#include <mecab.h>
#include <iostream>
#include <fstream>
#include <cstring>

#define CHECK(eval)                                                        \
	if (!eval)                                                             \
	{                                                                      \
		const char* e = tagger ? tagger->what() : MeCab::getTaggerError(); \
		std::cerr << "Exception:" << e << std::endl;                       \
		delete tagger;                                                     \
		return -1;                                                         \
	}

// Sample of MeCab::Tagger class.
int main(int argc, char** argv)
{
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
			std::cout << feature << "\n";
		}

		// std::cout << node->id << ' ';
		// if (node->stat == MECAB_BOS_NODE)
		// 	std::cout << "BOS";
		// else if (node->stat == MECAB_EOS_NODE)
		// 	std::cout << "EOS";
		// else
		// 	std::cout.write(node->surface, node->length);

		// std::cout << ' ' << node->feature << ' ' << (int)(node->surface - input) << ' '
		//           << (int)(node->surface - input + node->length) << ' ' << node->rcAttr << ' '
		//           << node->lcAttr << ' ' << node->posid << ' ' << (int)node->char_type << ' '
		//           << (int)node->stat << ' ' << (int)node->isbest << ' ' << node->alpha << ' '
		//           << node->beta << ' ' << node->prob << ' ' << node->cost << std::endl;
	}

	delete tagger;

	delete[] memblock;

	return 0;
}
