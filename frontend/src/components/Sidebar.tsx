import { Component, createEffect, createSignal, Show, useContext } from 'solid-js';
import About from './About';
import { A } from '@solidjs/router';
import {
  NavigationMenu,
  // NavigationMenuContent,
  // NavigationMenuIcon,
  NavigationMenuItem,
  // NavigationMenuLink,
  NavigationMenuTrigger
} from '~/components/ui/navigation-menu';

import { DynamicRouterContext, MxDynamicRouterContext } from './DynamicRouter';
import { DebugPanel } from '~/Debug';
import { MxSpinner } from '~/api';
import { gExpname } from '~/lib/globalstate';

const [devTools, setDevTools] = createSignal(false);

const Sidebar: Component = () => {

	const [showAbout, setShowAbout] = createSignal(false);
	const [showDevPopup, setShowDevPopup] = createSignal(false);
	const [devSteps, setDevSteps] = createSignal(9);
	const { addRoute } = useContext(DynamicRouterContext) as MxDynamicRouterContext;

	let timeout: NodeJS.Timeout;

	function checkDev() {
		setDevSteps(devSteps() - 1);

		if(devSteps() < 1) {
			setDevTools(true);
			addRoute('/debug', DebugPanel);
			return;
		}

		if(devSteps() < 8) {
			setShowDevPopup(true);
			clearTimeout(timeout);
			timeout = setTimeout(() => { setShowDevPopup(false); setDevSteps(9); }, 2000);
		}
	}

	return (
		<div class="top-0 m-0 border-r-2 hover:shadow-2xl shadow-lg border-solid border-black-200 min-h-full min-w-36 max-w-36" style="width:10%;position:fixed!important;z-index:1;overflow:auto;">
			<div class="flex flex-col items-center p-5">
				<span>
					<Show when={gExpname() != ''} fallback={<h1><b>mxfk</b></h1>}>
						<h1><b>{gExpname()}</b></h1>
					</Show>
				</span>
				<Show when={gExpname() == ''}>
					<div class="fixed inset-0 bg-black/50 backdrop-blur-sm z-50 flex items-center justify-center">
						<div class="bg-white rounded-md shadow-lg w-[90%] max-w-md p-5 flex flex-col items-center justify-center">
							<p class="font-bold mb-2">Loading Experiment</p>
							<MxSpinner description="Please Wait..."/>
						</div>
					</div>
				</Show>
				<div class="m-5">
					<NavigationMenu orientation="vertical">
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/'>Home</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/project'>Project</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/backends'>Backends</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/events'>Events</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/rdb'>RDB</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/history'>History</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								<A href='/dashboard'>Dashboard</A>
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<Show when={devTools()}>
							<NavigationMenuItem>
								<NavigationMenuTrigger>
									<A href='/dynamic/debug'>Debug</A>
								</NavigationMenuTrigger>
							</NavigationMenuItem>
						</Show>
						<NavigationMenuItem>
							<NavigationMenuTrigger onClick={() => setShowAbout(true)}>
								About
							</NavigationMenuTrigger>
						</NavigationMenuItem>
					</NavigationMenu>
				</div>
				<Show when={showDevPopup()}>
					<div class="fixed top-1/2 left-1/2 transform -translate-x-1/2 -translate-y-1/2 bg-gray-200 p-5 rounded-md shadow-lg">
						{ devTools() ? devSteps() > 1 ? "You are already a developer." : "You are now a developer." : `You are ${devSteps()} steps away from entering the developer mode.`}
					</div>
				</Show>
				<footer class="p-5" style="position:fixed; bottom:0">
					<div class="grid justify-items-center">
						<small>Version {__APP_VNAME__}</small>
						<small class="text-gray-400 cursor-default select-none" onClick={checkDev}>v{__APP_VERSION__}</small>
					</div>
				</footer>
			</div>
			<About show={showAbout()} setShow={setShowAbout}/>
		</div>
	);
};

export default Sidebar;
