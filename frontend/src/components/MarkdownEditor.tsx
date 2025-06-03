import { Component } from "solid-js";
import 'katex/dist/katex.min.css';
import 'highlight.js/styles/tomorrow-night-bright.min.css';
import markdownit from 'markdown-it';

// @ts-expect-error: No type declarations needed
import mk from 'markdown-it-katex';
import hljs from 'highlight.js';

export const MarkdownDisplay : Component<{ content: string }> = (props) => {
	const mdit = markdownit({
		html: true,
		breaks: true,
		typographer: true,
		linkify: true,
		highlight: (str: string, lang: string) => {
			if(lang && hljs.getLanguage(lang)) {
				try {
					return hljs.highlight(str, { language: lang }).value;
				}
				catch (_) {
				}
			}
			return '';
		}
	}).use(mk);

	return (
		<div class="p-5 prose prose-slate max-w-none w-full bg-white rounded-md shadow-md" innerHTML={mdit.render(props.content)}/>
	);
};
