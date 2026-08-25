package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.ui

import androidx.compose.animation.Crossfade
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.NavigateNext
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.withStyle
import org.jetbrains.compose.resources.stringResource
import ua.syt0r.kanji.Res
import ua.syt0r.kanji.presentation.common.AppTextField
import ua.syt0r.kanji.presentation.common.clickable
import ua.syt0r.kanji.presentation.common.copyCentered
import ua.syt0r.kanji.presentation.common.theme.Dimens
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.TextAnalysisContract
import ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.TextAnalysisInputState
import ua.syt0r.kanji.text_analysis_input_placeholder
import ua.syt0r.kanji.text_analysis_offer_subtitle
import ua.syt0r.kanji.text_analysis_offer_title

@Composable
fun TextAnalysisInputUI(
    state: State<TextAnalysisInputState>,
    navigateToAccount: () -> Unit
) {

    Column(
        modifier = Modifier
            .padding(top = Dimens.SpacingBig, bottom = Dimens.ContentPadding)
            .clip(MaterialTheme.shapes.medium)
            .background(MaterialTheme.colorScheme.surfaceDim)
            .height(IntrinsicSize.Max),
        verticalArrangement = Arrangement.spacedBy(Dimens.SpacingTiny)
    ) {

        val inputState = state.value

        val isLoading: Boolean
        val input: String
        val canEdit: Boolean
        val isInputValid: Boolean
        val canSubmit: Boolean

        when (inputState) {
            is TextAnalysisInputState.Loading -> {
                isLoading = true
                input = inputState.input
                canEdit = false
                isInputValid = true
                canSubmit = false
            }

            is TextAnalysisInputState.Typing -> {
                isLoading = false
                input = inputState.input.value
                canEdit = true
                isInputValid = inputState.isInputValid.value
                canSubmit = inputState.isInputValid.value
            }

            is TextAnalysisInputState.NotEligible -> {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid),
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(Dimens.ContentPaddingSmall)
                ) {
                    Column(
                        modifier = Modifier.weight(1f)
                    ) {
                        Text(
                            text = stringResource(Res.string.text_analysis_offer_title),
                            style = MaterialTheme.typography.titleMedium,
                            lineHeight = MaterialTheme.typography.titleMedium.fontSize
                        )
                        Text(
                            text = stringResource(Res.string.text_analysis_offer_subtitle),
                            style = MaterialTheme.typography.labelMedium
                        )
                    }

                    Icon(
                        imageVector = Icons.AutoMirrored.Default.NavigateNext,
                        contentDescription = null,
                        modifier = Modifier
                            .size(Dimens.IconButton)
                            .clip(MaterialTheme.shapes.medium)
                            .clickable(navigateToAccount)
                            .wrapContentSize()
                            .size(Dimens.Icon)
                    )
                }

                return@Column
            }
        }

        AppTextField(
            value = input,
            readOnly = !canEdit,
            onValueChange = {
                inputState as TextAnalysisInputState.Typing
                inputState.input.value = it
            },
            maxLines = 4,
            placeholderText = stringResource(Res.string.text_analysis_input_placeholder),
            decorationPaddings = PaddingValues(
                top = Dimens.ContentPadding,
                start = Dimens.ContentPadding,
                end = Dimens.ContentPadding
            ),
            modifier = Modifier.fillMaxWidth()
        )

        Row(
            modifier = Modifier.fillMaxWidth().padding(Dimens.SpacingMid),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid)
        ) {

            Text(
                text = buildAnnotatedString {
                    withStyle(
                        SpanStyle(
                            color = when {
                                isInputValid -> MaterialTheme.colorScheme.onSurfaceVariant
                                else -> MaterialTheme.colorScheme.error
                            }
                        )
                    ) { append(input.length.toString()) }
                    append("/${TextAnalysisContract.Companion.INPUT_LIMIT}")
                },
                style = MaterialTheme.typography.labelSmall.copyCentered(),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = Dimens.SpacingBig)
            )

            Spacer(Modifier.weight(1f))

            Crossfade(
                targetState = isLoading,
                modifier = Modifier
                    .size(Dimens.IconButton)
                    .clip(MaterialTheme.shapes.medium)
                    .clickable(canSubmit) {
                        inputState as TextAnalysisInputState.Typing
                        inputState.submit()
                    }
            ) { isLoading ->
                val modifier = Modifier.fillMaxSize().wrapContentSize()
                when {
                    isLoading -> CircularProgressIndicator(modifier.size(Dimens.Icon))
                    else -> Icon(
                        imageVector = Icons.AutoMirrored.Default.NavigateNext,
                        contentDescription = null,
                        modifier = modifier,
                        tint = when {
                            isInputValid -> MaterialTheme.colorScheme.onSurfaceVariant
                            else -> MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.38f)
                        }
                    )
                }

            }

        }

    }

}