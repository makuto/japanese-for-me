#include <assert.h>
#include <string.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <thread>

#include "curl/curl.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

static const char* ankiConnectURL = "http://localhost:8765";

// Assumptions made
// - Anki is running, with the AnkiConnect plugin installed and enabled
// - Simple cards have "Front" and "Back" fields
// - Advanced cards use field "Lemma" for the thing in the language you're learning and "English
//   Gloss" for the first language field

// 7PM is when I want to be done studying to focus on winding down
int hourStudyTimeEnds = 7 + 12;

CURL* curl_handle = nullptr;
bool curlVerbose = false;
bool curlRequestVerbose = false;
bool curlResponseStats = false;
bool curlResponseVerbose = false;

// I hate auto, so I made this instead
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

	if (curlRequestVerbose)
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
		if (curlResponseStats && resultCode == CURLE_OK && contentType)
		{
			std::cout << "Received type " << contentType << "\n";
			std::cout << receivedString.size() << " characters received\n";
		}

		if (curlResponseVerbose)
			std::cout << receivedString << "\n";
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

void syncAnkiToAnkiWeb()
{
	const char* jsonRequest = "{\"action\": \"sync\", \"version\": 6}";
	std::string result = ankiConnectRequest(curl_handle, jsonRequest);
	// if (result.empty())
		// return;
}

void getDueCards(rapidjson::Document& jsonResult)
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

	// const rapidjson::Value& resultValue = jsonResult["result"];
	// assert(resultValue.IsArray());
	// for (rapidjson::SizeType i = 0; i < resultValue.Size(); ++i)
	// 	std::cout << "[" << i << "] " << resultValue[i].GetInt64() << "\n";
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

std::string getCardQuizWord(const RapidJsonObject& card)
{
	std::string quizWord = "";
	int fieldOrder = card["fieldOrder"].GetInt();
	assert(card["fields"].IsObject());
	const rapidjson::Value& fieldsValue = card["fields"];
	assert(fieldsValue.IsObject());
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

	return quizWord;
}

void listDueCardsInfo(const rapidjson::Value& dueCardsInfo)
{
	assert(dueCardsInfo.IsArray());

	for (rapidjson::SizeType i = 0; i < dueCardsInfo.Size(); ++i)
	{
		const RapidJsonObject& currentCard = dueCardsInfo[i].GetObject();
		int fieldOrder = currentCard["fieldOrder"].GetInt();

		// Get quiz word
		std::string quizWord = getCardQuizWord(currentCard);

		std::cout << "[" << i << "] " << currentCard["note"].GetInt64() << "\n"
		          << "\tField order: " << fieldOrder << "\n"
		          << "\tModel: " << currentCard["modelName"].GetString() << "\n"
		          << "\tInterval: " << currentCard["interval"].GetInt64() << "\n"
		          << "\tQuiz word: " << quizWord << "\n"
		          << "\n\n";
	}
}

int main()
{
	std::cout << "Japanese For Me\nA vocabulary learning app by Macoy Madson.\n\n";

	std::chrono::steady_clock::time_point programStartTime = std::chrono::steady_clock::now();
	std::cout << "Syncing to AnkiWeb...";
	
	curl_global_init(CURL_GLOBAL_ALL);
	curl_handle = curl_easy_init();

	// Make sure we are working with up-to-date data
	syncAnkiToAnkiWeb();
	std::cout << "done\n";

	// listDecks();
	rapidjson::Document dueCardIds;
	getDueCards(dueCardIds);
	if (dueCardIds.HasMember("result"))
	{
		rapidjson::Document dueCards;
		getCardInfo(dueCards, dueCardIds["result"]);

		assert(dueCards.HasMember("result"));
		const rapidjson::Value& dueCardsArray = dueCards["result"];
		// listDueCardsInfo(dueCardsArray);

		int numCards = static_cast<int>(dueCardsArray.Size());
		if (numCards)
		{
			// The time math is gonna be a bit loosey goosey here, but that's fine in our case
			std::time_t currentTime;
			std::tm* currentTimeInfo;
			std::time(&currentTime);
			currentTimeInfo = std::localtime(&currentTime);
			// -1 from hours to count for minutes eating into the hour
			float hoursTimeLeft = (((hourStudyTimeEnds - currentTimeInfo->tm_hour - 1) * 60) +
			                       (60 - currentTimeInfo->tm_min)) /
			                      60.f;
			float secondsTimeLeft = (hoursTimeLeft * 60.f * 60.f);

			if (secondsTimeLeft > 0.f)
			{
				std::cout << "Currently "
				          << (currentTimeInfo->tm_hour > 12 ? currentTimeInfo->tm_hour - 12 :
				                                              currentTimeInfo->tm_hour)
				          << ":" << (currentTimeInfo->tm_min < 10 ? "0" : "")
				          << currentTimeInfo->tm_min << ", " << hoursTimeLeft
				          << " hours left to study " << numCards << " cards\n\n";

				std::cout << "100% accuracy:\n\t";
				std::cout << (secondsTimeLeft / numCards) << " seconds per card, "
				          << std::min(numCards / hoursTimeLeft, static_cast<float>(numCards))
				          << " cards per hour\n\n";

				std::cout << "80% accuracy:\n\t";
				std::cout << (secondsTimeLeft / (numCards * 1.2f)) << " seconds per card, "
				          << std::min((numCards * 1.2f) / hoursTimeLeft,
				                      static_cast<float>(numCards))
				          << " cards per hour\n";

				// Drip feed cards over the maximal time
				for (int i = 0; i < std::min(5, numCards); ++i)
				{
					// Present card
					const RapidJsonObject& currentCard = dueCardsArray[i].GetObject();
					std::string quizWord = getCardQuizWord(currentCard);
					std::cout << "[" << i + 1 << "/" << numCards << "]\n\t" << quizWord << "\n";

					// Wait to present next card
					std::chrono::steady_clock::time_point startTime =
					    std::chrono::steady_clock::now();
					// This won't be super accurate, but give or take a couple seconds even is fine
					// in our case. Keep it positive so things don't get too wacky if weird math is
					// above
					float timeToNextCard = std::max(0.1f, secondsTimeLeft / (numCards * 1.2f));
					std::this_thread::sleep_for(
					    std::chrono::duration<float, std::ratio<1, 1>>(timeToNextCard));
					std::chrono::steady_clock::time_point endTime =
					    std::chrono::steady_clock::now();

					std::chrono::duration<float> timeSlept =
					    std::chrono::duration_cast<std::chrono::duration<float>>(endTime -
					                                                             startTime);
					std::cout << "\n\n" << timeSlept.count() << " seconds slept\n";
				}
			}
			else
				std::cout << "Study time over. " << numCards << " cards remaining.\n";
		}
	}

	curl_easy_cleanup(curl_handle);
	curl_global_cleanup();
	return 0;
}
