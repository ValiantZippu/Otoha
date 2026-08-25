package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis

import androidx.compose.runtime.MutableState
import androidx.compose.runtime.State
import ua.syt0r.kanji.core.app_data.data.VocabReading


sealed interface TextAnalysisInputState {

    object NotEligible : TextAnalysisInputState

    data class Typing(
        val input: MutableState<String>,
        val isInputValid: State<Boolean>,
        val submit: () -> Unit
    ) : TextAnalysisInputState

    data class Loading(
        val input: String
    ) : TextAnalysisInputState

}

sealed interface TextAnalysisContentState {

    object Empty : TextAnalysisContentState

    data class Loaded(
        val contentMode: State<TextAnalysisContentMode>,
        val result: TextAnalysisResult,
    ) : TextAnalysisContentState

}

sealed interface TextAnalysisContentMode {

    data class Browse(
        val furigana: MutableState<Boolean>,
        val highlight: MutableState<Boolean>,
        val alternativeWords: Set<TextAnalysisNode.Word>,
        val switchToSaveLettersMode: () -> Unit
    ) : TextAnalysisContentMode

    data class SaveLetters(
        val letters: List<String>,
        val selected: State<Set<String>>,
        val toggleSelection: (String) -> Unit,
        val selectAll: () -> Unit,
        val selectNone: () -> Unit,
        val selectAllKanji: () -> Unit,
        val switchToBrowseMode: () -> Unit
    ) : TextAnalysisContentMode

}

sealed interface TextAnalysisResult {

    data class Success(
        val text: String,
        val translation: String,
        val nodeList: List<TextAnalysisNode>,
        val letters: List<String>,
        val alternativeWords: Set<TextAnalysisNode.Word>
    ) : TextAnalysisResult

    data class Error(
        val message: String
    ) : TextAnalysisResult

}

sealed interface TextAnalysisNode {

    data class Text(
        val value: String
    ) : TextAnalysisNode

    data class Word(
        val sequence: Long?,
        val text: String,
        val reading: VocabReading,
        val cards: List<CardData>,
        val highlightPartOfSpeech: PartOfSpeech?
    ) : TextAnalysisNode

    data class Compound(
        val childNodeList: List<TextAnalysisNode>
    ) : TextAnalysisNode

    data class Error(
        val text: String?
    ) : TextAnalysisNode

    data class AlternativeGroup(
        val childNodeList: List<TextAnalysisNode>
    ) : TextAnalysisNode

    data class CardData(
        val sequence: Long?,
        val reading: VocabReading,
        val notes: List<String>,
        val glossary: List<String>,
        val partOfSpeech: List<PartOfSpeech>
    )

    enum class PartOfSpeech(regexPattern: String) {
        Noun("^(n|n-adv|n-pr|n-pref|n-suf|n-t|adj-no)\$"),
        Verb("^v.*"),
        Adjective("^adj.*"),
        Particle("^prt\$"),
        Suffix("^suf\$"),
        Prefix("^pref\$"),
        Expression("^exp\$"),
        Counter("^ctr\$"),
        Interjection("^int\$"),
        Conjunction("^conj\$"),
        Pronoun("^pn\$"),
        Number("^num\$"),
        Adverb("^adv.*"),
        Auxiliary("^aux.*"),
        Copula("^cop\$"),
        Unclassified("^unc\$");

        val regex = Regex(regexPattern)

        companion object {

            fun detect(tag: String): PartOfSpeech? {
                return entries.firstOrNull { it.regex.matches(tag) }
            }

            fun detectAll(tags: List<String>): Set<PartOfSpeech> {
                return tags.mapNotNull { detect(it) }.toSet()
            }

        }
    }

}