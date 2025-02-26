/* @refresh reload */
import { render, Show } from 'solid-js/web';
import { Router, Route } from '@solidjs/router';
import { Toaster } from '~/components/ui/toast';

import './index.css';
import Home from './Home';
import { Project } from './Project';
import { RdbViewer } from './RdbViewer';
import { LogProvider } from './components/LogTable';
import { HistoryViewer } from './HistoryViewer';
import { EventsViewer } from './EventsViewer';
import { DynamicRouter, DynamicRouterProvider } from './components/DynamicRouter';
import { MetaProvider, Link } from '@solidjs/meta';

const root = document.getElementById('root');

if (import.meta.env.DEV && !(root instanceof HTMLElement)) {
	throw new Error(
		'Root element not found. Did you forget to add it to your index.html? Or maybe the id attribute got misspelled?',
	);
}

render(() => (
		<MetaProvider>
			<Link rel="icon" href="/favicon.ico"/>
			<DynamicRouterProvider>
				<LogProvider maxLogs={100}>
					<Router>
						<Route path='/' component={Home}/>
						<Route path='/project' component={Project}/>
						<Route path='/rdb' component={RdbViewer}/>
						<Route path='/history' component={HistoryViewer}/>
						<Route path='/events' component={EventsViewer}/>
						<DynamicRouter/>
					</Router>
					<Toaster/>
				</LogProvider>
			</DynamicRouterProvider>
		</MetaProvider>
	), root!);
