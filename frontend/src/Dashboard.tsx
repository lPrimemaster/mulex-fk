import { Component, Show, createSignal } from "solid-js";
import { DynamicTitle } from "./components/DynamicTitle";
import Sidebar from "./components/Sidebar";
import { gLoggedUser } from "./lib/globalstate";
import Card from "./components/Card";
import { MxButton } from "./api";
import { MxWebsocket } from "./lib/websocket";
import { MxGenericType } from "./lib/convert";
import { MxPopup } from "./components/Popup";
import DeleteIcon from './assets/delete.svg';
import KeyIcon from './assets/key.svg';
import AvatarIcon from './assets/avatar.svg';
import { MxFileDropUpload } from "./components/FileDropUpload";

export const Dashboard : Component = () => {
	const [accDelPopup, setAccDelPopup] = createSignal<boolean>(false);
	const [accChangePopup, setAccChangePopup] = createSignal<boolean>(false);
	const [accAvatarPopup, setAccAvatarPopup] = createSignal<boolean>(false);
	const [username, setUsername] = createSignal<string>('');
	const [oldPassword, setOldPassword] = createSignal<string>('');
	const [newPassword, setNewPassword] = createSignal<string>('');
	const [newPasswordConf, setNewPasswordConf] = createSignal<string>('');
	const [changeFailed, setChangeFailed] = createSignal<string>('');
	const [changeOK, setChangeOK] = createSignal<string>('');

	function changePassword(e: Event) {
		e.preventDefault();
		if(newPassword() != newPasswordConf()) {
			setChangeFailed("Passwords don't not match.");
			setTimeout(() => setChangeFailed(''), 5000);
			return;
		}
		
		MxWebsocket.instance.rpc_call('mulex::PdbUserChangePassword', [
			MxGenericType.str512(oldPassword()),
			MxGenericType.str512(newPassword())
		]).then((res: MxGenericType) => {
			if(!res.astype('bool')) {
				setChangeFailed("Old password incorrect.");
				setTimeout(() => setChangeFailed(''), 5000);
			}
			else {
				setChangeOK("Password changed.");
				setTimeout(() => setChangeOK(''), 5000);
				setNewPassword('');
				setNewPasswordConf('');
				setOldPassword('');
			}
		});
	}

	return (
		<div>
			<DynamicTitle title="Dashboard"/>
			<Sidebar/>
			<div class="p-5 ml-36 mr-auto">
				<div class="columns-1 xl:columns-2 gap-5">
					<Card title="Info">
						{gLoggedUser()}
					</Card>
					<Card title="Account Settings">
						<MxPopup title="Delete account?" open={accDelPopup()} onOpenChange={setAccDelPopup}>
							<div class="place-items-center">
								<div class="place-content-center justify-center font-semibold text-md">
									You are about to delete you account.
								</div>
								<div class="place-content-center justify-center font-semibold text-md">
									Type your username bellow to confirm.
								</div>
								<div class="place-content-center justify-center font-semibold text-lg mt-2 text-red-500">
									This action is irreversible.
								</div>
								<div class="flex my-5">
									<input
										type="text"
										value={username()}
										onInput={(e) => setUsername(e.currentTarget.value)}
										class="w-full border border-red-300 rounded-lg
											   px-4 mr-2 py-2 focus:outline-none focus:ring-2
											   focus:ring-red-500 text-red-500 placeholder-red-200"
										placeholder={gLoggedUser()}
										required
									/>
									<MxButton 
										type="error"
										onClick={() => MxWebsocket.instance.rpc_call('mulex::PdbUserDelete', [MxGenericType.str512(gLoggedUser())])}
										disabled={username() != gLoggedUser()}
									>
										Delete
									</MxButton>
								</div>
							</div>
						</MxPopup>

						<MxPopup title="Change password" open={accChangePopup()} onOpenChange={setAccChangePopup}>
							<div class="place-items-center">
								<Show when={changeFailed() !== ''}>
									<div class="flex justify-center mb-6 border border-red-900 bg-red-300 text-red-900 rounded-lg w-full py-2">
										{changeFailed()}
									</div>
								</Show>
								<Show when={changeOK() !== ''}>
									<div class="flex justify-center mb-6 border border-green-900 bg-green-300 text-green-900 rounded-lg w-full py-2">
										{changeOK()}
									</div>
								</Show>
								<form onSubmit={changePassword} class="space-y-4">
									<div>
										<label class="block text-gray-700 text-sm font-medium mb-1">Old Password</label>
										<input
											type="password"
											autocomplete="current-password"
											value={oldPassword()}
											onInput={(e) => setOldPassword(e.currentTarget.value)}
											class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
											required
										/>
									</div>
									<div>
										<label class="block text-gray-700 text-sm font-medium mb-1">New Password</label>
										<input
											type="password"
											autocomplete="new-password"
											value={newPassword()}
											onInput={(e) => setNewPassword(e.currentTarget.value)}
											class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
											required
										/>
									</div>
									<div>
										<label class="block text-gray-700 text-sm font-medium mb-1">Confirm New Password</label>
										<input
											type="password"
											autocomplete="new-password"
											value={newPasswordConf()}
											onInput={(e) => setNewPasswordConf(e.currentTarget.value)}
											class="w-full border border-gray-300 rounded-lg px-4 py-2 focus:outline-none focus:ring-2 focus:ring-blue-500"
											required
										/>
									</div>
									<button
										type="submit"
										class="w-full bg-blue-600 text-white py-2 rounded-lg hover:bg-blue-700 transition"
									>
										Change Password
									</button>
								</form>
							</div>
						</MxPopup>

						<MxPopup title="Change avatar" open={accAvatarPopup()} onOpenChange={setAccAvatarPopup}>
							<div class="place-items-center">
								<MxFileDropUpload
									limit={1}
									maxSize={1024 * 1024} // 1MB max
									allowedExtensions={['png', 'jpeg', 'jpg', 'gif']}
									onUploadCompleted={async (handles) => {
										const res = await MxWebsocket.instance.rpc_call('mulex::PdbUserChangeAvatar', [
											MxGenericType.str512(handles[0])
										]);

										if(!res.astype('bool')) {
											console.error('Failed to set user avatar');
										}

										setAccAvatarPopup(false); // Close this popup
									}}
								/>
							</div>
							<div class="place-items-center">
							</div>
						</MxPopup>
						<div class="flex gap-5">
							<MxButton onClick={() => setAccDelPopup(true)} type="error" class="size-20 place-items-center" disabled={gLoggedUser() === 'admin'}>
								{/* @ts-ignore */}
								<DeleteIcon class="size-10"/>
								<div class="text-xs font-semibold">Delete Account</div>
							</MxButton>
							<MxButton onClick={() => setAccChangePopup(true)} class="size-20 place-items-center">
								{/* @ts-ignore */}
								<KeyIcon class="size-10"/>
								<div class="text-xs font-semibold">Change Password</div>
							</MxButton>
							<MxButton onClick={() => setAccAvatarPopup(true)} class="size-20 place-items-center">
								{/* @ts-ignore */}
								<AvatarIcon class="size-10 fill-black"/>
								<div class="text-xs font-semibold">Change Avatar</div>
							</MxButton>
						</div>
					</Card>
				</div>
			</div>
		</div>
	);
};
