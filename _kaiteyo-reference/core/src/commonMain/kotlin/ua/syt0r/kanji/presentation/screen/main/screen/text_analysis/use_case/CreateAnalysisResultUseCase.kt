package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.use_case

import ua.syt0r.kanji.core.app_data.AppDataRepository
import ua.syt0r.kanji.core.japanese.isKana
import ua.syt0r.kanji.core.japanese.isKanji
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.TextAnalysisNode
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.TextAnalysisResult

interface CreateAnalysisResultUseCase {

    suspend operator fun invoke(
        text: String,
        translation: String,
        nodeList: List<TextAnalysisNode>
    ): TextAnalysisResult.Success

}

class DefaultCreateAnalysisResultUseCase(
    private val appDataRepository: AppDataRepository
) : CreateAnalysisResultUseCase {

    override suspend fun invoke(
        text: String,
        translation: String,
        nodeList: List<TextAnalysisNode>
    ): TextAnalysisResult.Success {
        return TextAnalysisResult.Success(
            text = text,
            translation = translation,
            nodeList = nodeList,
            letters = text.asSequence()
                .filter { it.isKana() || it.isKanji() }
                .map { it.toString() }
                .toList()
                .filter { appDataRepository.getStrokes(it).isNotEmpty() },
            alternativeWords = nodeList.asSequence()
                .filterIsInstance<TextAnalysisNode.AlternativeGroup>()
                .flatMap { it.childNodeList.drop(1) }
                .flatMap { it.words() }
                .toSet()
        )
    }

    private fun TextAnalysisNode.words(): List<TextAnalysisNode.Word> {
        return when (this) {
            is TextAnalysisNode.AlternativeGroup -> childNodeList.flatMap { it.words() }
            is TextAnalysisNode.Compound -> childNodeList.flatMap { it.words() }
            is TextAnalysisNode.Word -> listOf(this)
            is TextAnalysisNode.Text,
            is TextAnalysisNode.Error -> emptyList()
        }
    }

}