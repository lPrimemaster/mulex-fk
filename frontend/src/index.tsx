/* @refresh reload */
import { render } from 'solid-js/web';
import { Router, Route } from '@solidjs/router';
import { Toaster } from '~/components/ui/toast';

import './index.css';
import Home from './Home';
import { RdbViewer } from './RdbViewer';

const root = document.getElementById('root');

if (import.meta.env.DEV && !(root instanceof HTMLElement)) {
	throw new Error(
		'Root element not found. Did you forget to add it to your index.html? Or maybe the id attribute got misspelled?',
	);
}

render(() => (
		<>
			<Router>
				<Route path='/' component={Home}/>
				<Route path='/rdb' component={RdbViewer}/>
			</Router>
			<Toaster/>
		</>
	), root!);
