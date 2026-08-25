package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.sizeIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupProperties
import ua.syt0r.kanji.core.app_data.data.formattedVocabStringReading
import ua.syt0r.kanji.presentation.common.copyCentered
import ua.syt0r.kanji.presentation.common.theme.Dimens
import ua.syt0r.kanji.presentation.common.ui.VerticalScrollbar
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.TextAnalysisNode

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TextAnalysisVocabCardPopup(
    showPopup: MutableState<Boolean>,
    node: TextAnalysisNode.Word,
    saveWord: (TextAnalysisNode.CardData) -> Unit
) {

    if (showPopup.value.not()) return

    val scrollState = rememberScrollState()

    Popup(
        onDismissRequest = { showPopup.value = false },
        properties = PopupProperties()
    ) {
        Row(
            modifier = Modifier
                .height(IntrinsicSize.Max)
                .shadow(Dimens.SpacingMid, MaterialTheme.shapes.medium)
                .clip(MaterialTheme.shapes.medium)
                .background(MaterialTheme.colorScheme.surfaceDim)
        ) {

            Column(
                modifier = Modifier
                    .sizeIn(
                        minWidth = Dimens.PopupMinWidth,
                        maxWidth = Dimens.PopupMaxSize,
                        maxHeight = Dimens.PopupMaxSize
                    )
                    .verticalScroll(scrollState)
                    .padding(vertical = Dimens.ContentPaddingSmall)
                    .padding(start = Dimens.ContentPaddingSmall),
                verticalArrangement = Arrangement.spacedBy(Dimens.SpacingSmall)
            ) {

                node.cards.forEachIndexed { i, it ->

                    CardDataUI(
                        cardData = it,
                        onAddClick = { saveWord(it) }
                    )

                    if (i != node.cards.size - 1) {
                        Spacer(modifier = Modifier.height(Dimens.SpacingBig))
                    }

                }

            }

            VerticalScrollbar(
                scrollState = scrollState,
                modifier = Modifier
                    .fillMaxHeight()
                    .padding(vertical = Dimens.SpacingMid)
                    .padding(end = Dimens.SpacingTiny)
            )

        }

    }
}

@Composable
fun CardDataUI(
    cardData: TextAnalysisNode.CardData,
    onAddClick: () -> Unit
) {

    FlowRow(
        horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid)
    ) {
        val reading = cardData.reading
        Text(
            text = formattedVocabStringReading(
                kanaReading = reading.kanaReading,
                kanjiReading = reading.kanjiReading
            ),
            style = MaterialTheme.typography.bodyLarge.copyCentered(),
            modifier = Modifier
                .weight(1f)
                .align(Alignment.CenterVertically)
        )
        Icon(
            imageVector = Icons.Outlined.Add,
            contentDescription = null,
            modifier = Modifier
                .align(Alignment.CenterVertically)
                .clip(MaterialTheme.shapes.small)
                .clickable(onClick = onAddClick)
                .padding(Dimens.SpacingTiny)
        )
    }

    cardData.notes.takeIf { it.isNotEmpty() }?.let { notes ->
        Text(
            text = notes.joinToString(),
            style = MaterialTheme.typography.labelMedium
        )
    }

    if (cardData.partOfSpeech.isNotEmpty()) {
        FlowRow(horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid)) {
            cardData.partOfSpeech.forEach {
                val highlightColor = it.toHighlightColor(MaterialTheme.colorScheme.surfaceVariant)

                Text(
                    text = it.name,
                    style = MaterialTheme.typography.labelSmall,
                    modifier = Modifier
                        .clip(RoundedCornerShape(Dimens.SpacingTiny))
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .drawWithContent {
                            drawContent()
                            val highlightHeightPx = Dimens.SpacingTiny.toPx()
                            drawRect(
                                color = highlightColor,
                                topLeft = Offset(0f, size.height - highlightHeightPx),
                                size = size.copy(height = highlightHeightPx)
                            )
                        }
                        .padding(vertical = Dimens.SpacingTiny)
                        .padding(
                            horizontal = Dimens.SpacingMid,
                            vertical = Dimens.SpacingSmall
                        )
                        .alignByBaseline()
                )
            }
        }
    }

    Spacer(modifier = Modifier.height(Dimens.SpacingMid))

    cardData.glossary.forEachIndexed { index, definition ->
        Row(
            horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid)
        ) {
            val style = MaterialTheme.typography.bodySmall
            Text(index.plus(1).toString(), style = style)
            Text(definition, Modifier.weight(1f), style = style)
        }
    }

}