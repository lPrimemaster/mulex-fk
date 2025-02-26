import { Component, createSignal, Show } from 'solid-js';
import About from './About';
import { A } from '@solidjs/router';
import {
  NavigationMenu,
  NavigationMenuContent,
  NavigationMenuIcon,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger
} from '~/components/ui/navigation-menu';

import metadata from '../lib/metadata';

const Sidebar: Component = () => {

	const [showAbout, setShowAbout] = createSignal(false);

	return (
		<div class="m-0 border-r-2 hover:shadow-2xl shadow-lg border-solid border-black-200 min-h-full min-w-36 max-w-36" style="width:10%;position:fixed!important;z-index:1;overflow:auto;">
			<div class="flex flex-col items-center p-5">
				<span>
					<Show when={!metadata.expname.loading} fallback={<h1><b>mxfk</b></h1>}>
						<h1><b>{metadata.expname()}</b></h1>
					</Show>
				</span>
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
							<NavigationMenuTrigger onClick={() => setShowAbout(true)}>
								About
							</NavigationMenuTrigger>
						</NavigationMenuItem>
					</NavigationMenu>
				</div>
				<footer class="p-5" style="position:fixed; bottom:0"><small>Version {__APP_VNAME__}</small></footer>
			</div>
			<About show={showAbout()} setShow={setShowAbout}/>
		</div>
	);
};

export default Sidebar;
