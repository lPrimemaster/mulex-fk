import type { Component } from 'solid-js';
import {
  NavigationMenu,
  NavigationMenuContent,
  NavigationMenuIcon,
  NavigationMenuItem,
  NavigationMenuLink,
  NavigationMenuTrigger
} from "~/components/ui/navigation-menu"

const Sidebar: Component = () => {
	return (
		<div class="m-0 border-r-2 border-solid border-blue-200 min-h-full" style="width:10%;position:fixed!important;z-index:1;overflow:auto;">
			<div class="flex flex-col items-center m-5">
				<span>
					<h1><b>MX-FK</b></h1>
				</span>
				<div class="m-5">
					<NavigationMenu orientation="vertical">
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								Home
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								Event Viewer
							</NavigationMenuTrigger>
						</NavigationMenuItem>
						<NavigationMenuItem>
							<NavigationMenuTrigger>
								About
							</NavigationMenuTrigger>
						</NavigationMenuItem>
					</NavigationMenu>
				</div>
				<footer class="p-5" style="position:fixed; bottom:0"><small>Version Centauri</small></footer>
			</div>
		</div>
	);
};

export default Sidebar;
