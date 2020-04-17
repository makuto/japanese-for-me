#include <assert.h>
#include <string.h>
#include <iostream>
#include <sstream>

#include "curl/curl.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

static const char* ankiConnectURL = "http://localhost:8765";

CURL* curl_handle = nullptr;
bool curlVerbose = false;

// I hate auto
typedef rapidjson::GenericObject<
    true, rapidjson::GenericValue<rapidjson::UTF8<char>,
                                  rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>>>
    RapidJsonObject;

// Note that this can be called many times for a single request, if the packets are split
static size_t CurlReceive(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	std::string* stringOut = static_cast<std::string*>(userdata);
	stringOut->append(ptr);
	return (size_t)(size * nmemb);
}

std::string ankiConnectRequest(CURL* curl_handle, const char* jsonRequest)
{
	std::string receivedString = "";

	std::cout << "Request: '" << jsonRequest << "'\n";

	curl_easy_setopt(curl_handle, CURLOPT_URL, ankiConnectURL);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, CurlReceive);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)&receivedString);
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Japanese-for-me/1.0");
	if (curlVerbose)
		curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
	// curl_easy_setopt(curl_handle, CURLOPT_RETURNTRANSFER, true);

	curl_slist* httpHeaders = nullptr;
	httpHeaders = curl_slist_append(httpHeaders, "Expect:");
	httpHeaders = curl_slist_append(httpHeaders, "Content-Type: application/json");
	httpHeaders = curl_slist_append(httpHeaders, "Accept: text/json");
	httpHeaders = curl_slist_append(httpHeaders, "charset: utf-8");
	curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, httpHeaders);

	curl_easy_setopt(curl_handle, CURLOPT_POST, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, strlen(jsonRequest));
	curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, jsonRequest);

	CURLcode resultCode = curl_easy_perform(curl_handle);
	if (resultCode == CURLE_OK)
	{
		char* contentType;
		resultCode = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &contentType);
		if (resultCode == CURLE_OK && contentType)
			std::cout << "Received type " << contentType << "\n";

		std::cout << receivedString.size() << " characters received\n";
		// std::cout << receivedString << "\n";
	}
	else
	{
		std::cerr << "Error: " << curl_easy_strerror(resultCode) << "\n";
	}

	return receivedString;
}

void listDecks()
{
	const char* jsonRequest = "{\"action\": \"deckNames\", \"version\": 6}";
	std::string result = ankiConnectRequest(curl_handle, jsonRequest);
	if (result.empty())
		return;
	rapidjson::Document jsonResult;
	jsonResult.Parse(result.c_str());
	const rapidjson::Value& resultValue = jsonResult["result"];
	assert(resultValue.IsArray());
	for (rapidjson::SizeType i = 0; i < resultValue.Size(); ++i)
		std::cout << "[" << i << "] " << resultValue[i].GetString() << "\n";
}

void listDueCards(rapidjson::Document& jsonResult)
{
	// Build the request
	rapidjson::StringBuffer jsonString;
	rapidjson::Writer<rapidjson::StringBuffer> writer(jsonString);
	writer.StartObject();
	writer.Key("action");
	writer.String("findCards");
	writer.Key("version");
	writer.Int(6);

	writer.Key("params");
	{
		writer.StartObject();
		writer.Key("query");
		std::ostringstream query;
		query << "is:due";
		writer.String(query.str().c_str());
		writer.EndObject();
	}
	writer.EndObject();

	const char* jsonRequest = jsonString.GetString();
	std::string result = ankiConnectRequest(curl_handle, jsonRequest);
	if (result.empty())
		return;
	jsonResult.Parse(result.c_str());
	const rapidjson::Value& resultValue = jsonResult["result"];
	assert(resultValue.IsArray());
	for (rapidjson::SizeType i = 0; i < resultValue.Size(); ++i)
		std::cout << "[" << i << "] " << resultValue[i].GetInt64() << "\n";
}

void getCardInfo(rapidjson::Document& jsonResult, const rapidjson::Value& cardsIds)
{
	rapidjson::StringBuffer jsonString;
	rapidjson::Writer<rapidjson::StringBuffer> writer(jsonString);
	writer.StartObject();
	writer.Key("action");
	writer.String("cardsInfo");
	writer.Key("version");
	writer.Int(6);

	writer.Key("params");
	{
		writer.StartObject();
		writer.Key("cards");
		writer.StartArray();
		for (rapidjson::SizeType i = 0; i < cardsIds.Size(); ++i)
			writer.Int64(cardsIds[i].GetInt64());
		writer.EndArray();
		writer.EndObject();
	}
	writer.EndObject();

	const char* jsonRequest = jsonString.GetString();
	// std::cout << jsonRequest << "\n";
	std::string result = ankiConnectRequest(curl_handle, jsonRequest);
	if (result.empty())
		return;

	jsonResult.Parse(result.c_str());
}

void printObjectMembers(const rapidjson::Value& objectValue)
{
	static const char* kTypeNames[] = {"Null",  "False",  "True",  "Object",
	                                   "Array", "String", "Number"};
	for (rapidjson::Value::ConstMemberIterator itr = objectValue.MemberBegin();
	     itr != objectValue.MemberEnd(); ++itr)
	{
		std::cout << itr->name.GetString() << " is " << kTypeNames[itr->value.GetType()] << "\n";
	}
}

int main()
{
	std::cout << "Japanese For Me\n";

	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();

	listDecks();
	rapidjson::Document dueCardIds;
	listDueCards(dueCardIds);
	if (dueCardIds.HasMember("result"))
	{
		rapidjson::Document dueCards;
		getCardInfo(dueCards, dueCardIds["result"]);

		assert(dueCards.HasMember("result"));
		const rapidjson::Value& resultValue = dueCards["result"];
		assert(resultValue.IsArray());
		for (rapidjson::SizeType i = 0; i < resultValue.Size(); ++i)
		{
			const RapidJsonObject& currentCard = resultValue[i].GetObject();
			int fieldOrder = currentCard["fieldOrder"].GetInt();

			// Get quiz word
			assert(currentCard["fields"].IsObject());
			const rapidjson::Value& fieldsValue = currentCard["fields"];
			assert(fieldsValue.IsObject());
			std::string quizWord = "";
			// Only "A Frequency Dictionary of Japanese Words" has this
			if (fieldsValue.HasMember("Lemma"))
			{
				if (fieldOrder == 1)
				{
					// English -> Japanese
					quizWord = fieldsValue["English Gloss"]["value"].GetString();
				}
				else
				{
					// Japanese -> English
					quizWord = fieldsValue["Lemma"]["value"].GetString();
				}
			}
			else if (fieldsValue.HasMember("Front"))
			{
				if (fieldOrder == 1)
					quizWord = fieldsValue["Front"]["value"].GetString();
				else
					quizWord = fieldsValue["Back"]["value"].GetString();
			}
			else
			{
				std::cerr << "Card has unrecognized fields\n";
			}

			// const rapidjson::Value& lemmaValue = currentCard["fields"]["Lemma"];
			// assert(lemmaValue.IsObject());
			// std::string lemmaString = resultValue[i]
			//                               .GetObject()["fields"]
			//                               .GetObject()["Lemma"]
			//                               .GetObject()["value"]
			//                               .GetString();

			std::cout << "[" << i << "] " << currentCard["note"].GetInt64() << "\n"
			          << "\tField order: " << fieldOrder << "\n"
			          << "\tModel: " << currentCard["modelName"].GetString() << "\n"
			          << "\tInterval: " << currentCard["interval"].GetInt64() << "\n"
			          << "\tQuiz word: " << quizWord << "\n"
			          << "\n\n";
		}
	}

	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	return 0;
}
