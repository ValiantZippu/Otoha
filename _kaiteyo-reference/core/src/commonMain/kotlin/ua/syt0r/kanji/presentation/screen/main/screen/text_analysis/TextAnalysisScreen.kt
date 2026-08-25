package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis

import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import ua.syt0r.kanji.presentation.getMultiplatformViewModel
import ua.syt0r.kanji.presentation.screen.main.MainDestination
import ua.syt0r.kanji.presentation.screen.main.MainNavigationState
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.ui.TextAnalysisScreenUI
import ua.syt0r.kanji.presentation.screen.main.screen.vocab_card.SuggestedVocabCardData
import ua.syt0r.kanji.presentation.screen.main.screen.vocab_card.VocabCardScreenMode


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun TextAnalysisScreen(
    navigationState: MainNavigationState,
    viewModel: TextAnalysisViewModel = getMultiplatformViewModel()
) {

    val screenState = viewModel.state.collectAsState()

    TextAnalysisScreenUI(
        screenState = screenState,
        saveWord = { cardData ->
            val reading = cardData.reading
            val destination = MainDestination.VocabCard(
                screenMode = VocabCardScreenMode.Save,
                cardData = SuggestedVocabCardData(
                    kanjiReading = reading.kanjiReading,
                    kanaReading = reading.kanaReading,
                    meaning = null,
                    alternativeMeanings = cardData.glossary,
                    jmDictId = cardData.sequence,
                    cardId = null
                )
            )
            navigationState.navigate(destination)
        },
        navigateBack = { navigationState.navigateBack() },
        navigateToAccount = { navigationState.navigate(MainDestination.Account()) }
    )

}
