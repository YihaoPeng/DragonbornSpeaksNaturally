using System;
using System.Collections.Generic;
using System.Speech.Recognition;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Text.RegularExpressions;

namespace DSN
{
    class RecognitionPhrase
    {
        public List<string> Words;
        public bool Optional;
        public bool SubsetMatching;

        public RecognitionPhrase(string words, bool optional, bool subsetMatching)
        {
            Words = new List<string>();
            Words.Add(words);
            Optional = optional;
            SubsetMatching = subsetMatching;
        }
        public RecognitionPhrase(List<string> words, bool optional, bool subsetMatching)
        {
            Words = words;
            Optional = optional;
            SubsetMatching = subsetMatching;
        }
    }

    class RecognitionGrammar
    {
        public string Name;

        private Configuration config;
        private List<RecognitionPhrase> phrases;
        private bool compiled;
        private Grammar microsoftGrammar;
        private string jsgf;

        private static Regex nonWordRegex = new Regex(@"[^\p{L}0-9.+-]");
        private static Regex numberRegex = new Regex("[+-]?[0-9]*(?:\\.[0-9]+)*");

        public RecognitionGrammar(Configuration config) {
            phrases = new List<RecognitionPhrase>();
            this.config = config;
        }

        public bool FromSRGS(string filePath) {
            // Only Microsoft Speech Recognition Engine supports it
            if (config.GetRecognitionEngine() != RecognitionEngine.Microsoft) {
                Trace.TraceError("Speech engine does not support SRGS, skipping {0}", filePath);
                return false;
            }

            // load a SRGS XML file
            XmlDocument doc = new XmlDocument();
            doc.Load(filePath);

            // If xml:lang in the file does not match the DSN's locale, the grammar cannot be loaded.
            XmlAttribute xmlLang = doc.CreateAttribute("xml:lang");
            xmlLang.Value = config.GetLocale();
            doc.DocumentElement.SetAttributeNode(xmlLang);

            MemoryStream xmlStream = new MemoryStream();
            doc.Save(xmlStream);
            xmlStream.Flush(); //Adjust this if you want read your data 
            xmlStream.Position = 0;

            microsoftGrammar = new Grammar(xmlStream);
            compiled = true;
            return true;
        }

        public void Append(string words, bool optional, bool subsetMatching) {
            compiled = false;
            phrases.Add(new RecognitionPhrase(words, optional, subsetMatching));
        }
        public void Append(List<string> words, bool optional, bool subsetMatching) {
            compiled = false;
            phrases.Add(new RecognitionPhrase(words, optional, subsetMatching));
        }

        public void Compile() {
            if (compiled) return;
            switch (config.GetRecognitionEngine())
            {
                case RecognitionEngine.Voice2Json:
                    List<string> parts = new List<string>();
                    foreach (RecognitionPhrase phrase in phrases) {
                        if (config.NeedSegmenter()) {
                            for (int i = 0; i < phrase.Words.Count; i++) {
                                // Remove symbols that the recognizer can't handle
                                phrase.Words[i] = nonWordRegex.Replace(phrase.Words[i], " ");
                                // Number to Chinese words
                                phrase.Words[i] = numberRegex.Replace(phrase.Words[i],
                                    str => ChineseNumerals.Number2ChineseJSGF(str.Value));
                                // Chinese word segmentation
                                phrase.Words[i] = Phrases.wordCut(phrase.Words[i]);
                                // Remove extra spaces
                                phrase.Words[i] = Phrases.cleanBlank(phrase.Words[i]);
                            }
                        }
                        if (config.LowercasePhrases()) {
                            for (int i = 0; i < phrase.Words.Count; i++) {
                                phrase.Words[i] = phrase.Words[i].ToLower();
                            }
                        }

                        string words;
                        if (phrase.Words.Count > 1) {
                            words = "( " + String.Join(" | ", phrase.Words) + " )";
                        } else {
                            words = phrase.Words[0];
                        }
                        if (phrase.Optional) {
                            parts.Add("[ " + words + " ]");
                        } else {
                            parts.Add(words);
                        }
                    }
                    jsgf = Phrases.cleanBlank(String.Join(" ", parts));
                    // 去除多余空白
                    
                    break;
                case RecognitionEngine.Microsoft:
                    GrammarBuilder builder = new GrammarBuilder();
                    foreach (RecognitionPhrase phrase in phrases) {
                        dynamic words;
                        if (phrase.Words.Count > 1) {
                            words = new Choices(phrase.Words.ToArray());
                        } else {
                            words = phrase.Words[0];
                        }
                        
                        if (phrase.Optional) {
                            builder.Append(words, 0, 1);
                        } else {
                            if (phrase.SubsetMatching) {
                                builder.Append(words, config.GetConfiguredMatchingMode());
                            } else {
                                builder.Append(words);
                            }
                        }
                    }
                    microsoftGrammar = new Grammar(builder);
                    break;
            }
            compiled = true;
        }

        public Grammar ToMicrosoftGrammar() {
            Compile();
            return microsoftGrammar;
        }

        public string ToJSGF() {
            Compile();
            return jsgf;
        }
    }
}
