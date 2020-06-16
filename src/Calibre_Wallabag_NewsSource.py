#!/usr/bin/env python2
# vim:fileencoding=utf-8
from __future__ import unicode_literals, division, absolute_import, print_function
from calibre.web.feeds.news import BasicNewsRecipe

class AdvancedUserRecipe1592324084(BasicNewsRecipe):
    title          = 'Wallabag Japanese'
    oldest_article = 365
    max_articles_per_feed = 100
    auto_cleanup   = True

    feeds          = [
        ('Wallabag', 'https://app.wallabag.it/feed/Macoy/hU7xNZIj2hRlyKk/unread'),
    ]
    
    def parse_feeds(self):
        # Call parent
        feeds = BasicNewsRecipe.parse_feeds(self)
        
        # Further filtering
        for feed in feeds:
            for article in feed.articles[:]:
                foundJapanese = False
                for char in article.title:
                    # While I could use <category> from Wallabag, I can't be assed
                    if is_cjk(char):
                        foundJapanese = True
                        break
                if not foundJapanese:
                    feed.articles.remove(article)
        return feeds

# Modified by Macoy Madson. Original copied from
# https://stackoverflow.com/questions/30069846/how-to-find-out-chinese-or-japanese-character-in-a-string-in-python

hiraganaRange = {'from': ord(u'\u3040'), 'to': ord(u'\u309f')} # Japanese Hiragana
katakanaRange = {"from": ord(u"\u30a0"), "to": ord(u"\u30ff")} # Japanese Katakana

# Specifically ignore Hiragana and Katakana in order to get all Kanji
cjkNonKanaRanges = [
    {"from": ord(u"\u3300"), "to": ord(u"\u33ff")},         # compatibility ideographs
    {"from": ord(u"\ufe30"), "to": ord(u"\ufe4f")},         # compatibility ideographs
    {"from": ord(u"\uf900"), "to": ord(u"\ufaff")},         # compatibility ideographs
    {"from": ord(u"\U0002F800"), "to": ord(u"\U0002fa1f")}, # compatibility ideographs
    {"from": ord(u"\u2e80"), "to": ord(u"\u2eff")},         # cjk radicals supplement
    {"from": ord(u"\u4e00"), "to": ord(u"\u9fff")},
    {"from": ord(u"\u3400"), "to": ord(u"\u4dbf")},
    {"from": ord(u"\U00020000"), "to": ord(u"\U0002a6df")},
    {"from": ord(u"\U0002a700"), "to": ord(u"\U0002b73f")},
    {"from": ord(u"\U0002b740"), "to": ord(u"\U0002b81f")},
    {"from": ord(u"\U0002b820"), "to": ord(u"\U0002ceaf")}  # included as of Unicode 8.0
]

cjkRanges = []
for nonKanaRange in cjkNonKanaRanges:
    cjkRanges.append(nonKanaRange)
cjkRanges.append(hiraganaRange)         # Japanese Hiragana
cjkRanges.append(katakanaRange)         # Japanese Katakana

# "The Alphabet"
latinRanges = [
    {"from": ord(u"\u0042"), "to": ord(u"\u005a")}, # Uppercase A-Z
    {"from": ord(u"\u0061"), "to": ord(u"\u007a")} # Lowercase a-z
]
  
# Is Chinese, Japanese, or Korean unicode character
def is_cjk(char):
    return any([range["from"] <= ord(char) <= range["to"] for range in cjkRanges])
