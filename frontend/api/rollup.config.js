import postcss from 'rollup-plugin-postcss';
import withSolid from 'rollup-preset-solid';

export default withSolid({
	input: "src/index.tsx",
	targets: ["esm", "cjs"],
	printInstructions: true,

	plugins: [
		postcss({
			plugins: [require('tailwindcss')]
		})
	]
});
