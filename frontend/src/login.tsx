/* @refresh reload */
import { render } from 'solid-js/web';
import './index.css';
import { Component, createSignal, onMount, Show } from 'solid-js';
import { MetaProvider, Title } from '@solidjs/meta';

const root = document.getElementById('root');

if (import.meta.env.DEV && !(root instanceof HTMLElement)) {
	throw new Error(
		'Root element not found. Did you forget to add it to your index.html? Or maybe the id attribute got misspelled?',
	);
}

const WarnBanner : Component<{ show: boolean, message: string }> = (props) => {
	return (
		<Show when={props.show}>
			<div class="fixed top-0 left-0 right-0 bg-red-300 text-red-900 text-sm font-semibold text-center p-3 z-30 shadow">
				{props.message}
			</div>
		</Show>
	);
};

const VersionFooter : Component = () => {
	return (
		<div class="fixed bottom-0 left-0 right-0 bg-transparent text-sm font-semibold text-center p-3 z-10">
			{__APP_VNAME__} v{__APP_VERSION__}<br/>{__APP_GBRANCH__}-{__APP_GHASH__}
		</div>
	);
};

const App : Component = () => {
	const [username, setUsername] = createSignal<string>("");
	const [password, setPassword] = createSignal<string>("");
	const [expname, setExpname]   = createSignal<string>("");
	const [showWarn, setShowWarn] = createSignal<boolean>(false);
	const [failed, setFailed] = createSignal<boolean>(false);

	let ftimeout: undefined | NodeJS.Timeout = undefined;

	onMount(async () => {
		const prpc = await fetch('/api/public', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ info: 'expname' })
		});
		const ret = await prpc.json();
		setExpname(ret['return']);

		setShowWarn(window.location.protocol != 'https:');
	});

	async function loginRequest(e: Event) {
		e.preventDefault();

		const loginResponse = await fetch('/api/login', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ username: username(), password: password() })
		});

		if(loginResponse.ok) {
			// We want a refresh so we can GET the new root
			window.location.reload();
		}
		else {
			setFailed(true);
			if(ftimeout) clearTimeout(ftimeout);
			ftimeout = setTimeout(() => setFailed(false), 5000);
			console.error(await loginResponse.text());
		}

		// TODO: (Cesar) JWTs are stateless - A deleted user has access
		// 				 until token expiration date
	}

	return (
		<div class="min-h-screen flex items-center justify-center bg-gray-100">
			<MetaProvider>
				<Title>Login • {expname()}</Title>
			</MetaProvider>
			<WarnBanner
				show={showWarn()}
				message={
					"HTTPS is disabled. User authentication and data are not TLS protected. Use with care! " +
					"You should not use this unless behind a VPN/Firewall! Refer to the docs on how to setup a reverse proxy."
				}
			/>
			<div class="w-full max-w-md bg-white rounded-2xl shadow-lg p-8 z-20">
				<div class="flex justify-center mb-6">
					<img src="/logo.png" class="w-full h-auto"/>
				</div>
				<h2 class="text-2xl font-semibold text-center mb-6">Login to {expname()}</h2>
				<Show when={failed()}>
					<div class="flex justify-center mb-6 border border-red-900 bg-red-300 text-red-900 rounded-lg w-full py-2">
						Invalid Credentials
					</div>
				</Show>
				<form onSubmit={loginRequest} class="space-y-4">
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Username</label>
						<input
							type="text"
							autocomplete="username"
							value={username()}
							onInput={(e) => setUsername(e.currentTarget.value)}
							class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
							required
						/>
					</div>
					<div>
						<label class="block text-gray-700 text-sm font-medium mb-1">Password</label>
						<input
							type="password"
							autocomplete="current-password"
							value={password()}
							onInput={(e) => setPassword(e.currentTarget.value)}
							class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
							required
						/>
					</div>
					<button
						type="submit"
						class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
					>
						Log In
					</button>
				</form>
			</div>
			<VersionFooter/>
		</div>
	);
};

render(() => <App/>, root!);
